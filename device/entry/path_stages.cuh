#pragma once

// Per-bounce / per-sample "stages" of the megakernel path integrator, factored out of
// __raygen__rg (entry/raygen.cuh) so the kernel body reads as a short sequence of named steps.
//
// Every function here is __device__ __forceinline__: the compiler inlines them straight back
// into the single raygen megakernel, so this is a *source-level* decomposition only. The
// megakernel structure, the per-ray state, and the generated-code behaviour are unchanged --
// each extraction is verified bit-identical against a banked render before it is kept.

#include "core/random.cuh"
#include "core/sampling.cuh"

#include "thesis/common/utils/math.h"
#include "thesis/device/payloads/miss.h"

namespace thesis {
namespace device {

// Russian roulette, applied once the path reaches rr_depth_. Returns true if the path should
// terminate at this bounce; on survival it rescales throughput by 1/p_survive (unbiased). Before
// rr_depth_ it is a no-op that draws no random number, so the RNG stream is identical to the
// inline form. The `p_survive <= 0` guard also kills a zero-throughput path, which would otherwise
// reach 0/0 = NaN in the rescale.
__device__ __forceinline__ bool russian_roulette(float3& throughput, size_t bounce,
                                                 random::PCG32& rng) {
    if (bounce < launch_params.render_.rr_depth_) {
        return false;
    }
    const auto p_survive =
        math::min(launch_params.render_.rr_max_survival_, math::max(throughput));
    if (random::sample_uniform(rng) > p_survive || p_survive <= 0.0f) {
        return true;
    }
    throughput /= p_survive;
    return false;
}

// Per-sample radiance finalisation, applied once the bounce loop ends.
//
// (1) Optional firefly suppression (beauty/robustness, OFF by default). Hue-preserving per-sample
// luminance clamp: if this sample's luminance exceeds the threshold, scale RGB down to the
// threshold (kills low-probability high-weight spikes while keeping colour). BIASED (removes energy
// from clamped pixels) -> opt-in, never for validation. Runtime-gated so it is a true no-op
// (bit-identical) when the threshold is 0 (the validation default).
//
// (2) Non-finite sample rejection (production safety net). Degenerate Gaussians in dense real-world
// assets can leak a NaN/Inf through an NEE / transmittance / phase term that the per-bounce
// throughput check does not catch (it guards throughput, not accumulated radiance). Such a sample is
// a numerical failure, not a real contribution -- zero it so it can't poison the pixel. FINITE
// samples are unchanged, so validation scenes stay bit-identical.
__device__ __forceinline__ void finalize_sample(float3& radiance) {
    if (launch_params.render_.firefly_clamp_luminance_ > 0.0f) {
        const float lum = math::dot(radiance, make_float3(0.2126f, 0.7152f, 0.0722f));
        if (lum > launch_params.render_.firefly_clamp_luminance_) {
            radiance *= launch_params.render_.firefly_clamp_luminance_ * math::rcp(lum);
        }
    }
    if (!isfinite(math::sum(radiance))) {
        radiance = make_float3(0.0f);
    }
}

// Shader Execution Reordering (Ada+; a no-op on Ampere/Turing, and compiled out entirely unless
// -DTHESIS_ENABLE_SER). Regroups the warp's threads by *where* they scattered so the downstream
// albedo / NEE-shadow / next-bounce traces run coherently (one shading path, coherent Primitive
// loads). Image bit-identical (pure scheduling) -- the divergence lever the Ampere 3090 lacks.
// Build-time variants: SER_CELL_INV (hint cell size), THESIS_SER_NOHINT (group by hit only),
// THESIS_SER_BOUNCE0 (reorder only at the first, most divergent bounce). Empty in the default build.
__device__ __forceinline__ void reorder_by_scatter_cell(float3 scatter_pos, bool result,
                                                        size_t bounce) {
#ifdef THESIS_ENABLE_SER
#ifndef SER_CELL_INV
#define SER_CELL_INV 8.0f
#endif
#ifdef THESIS_SER_BOUNCE0
    if (bounce != 0) {
        return;  // reorder only at the first (most divergent) bounce
    }
#else
    (void)bounce;
#endif
#ifdef THESIS_SER_NOHINT
    (void)scatter_pos;
    (void)result;
    optixReorder();
#else
    unsigned int ser_hint = 0u;
    if (result) {
        const float inv_cell = SER_CELL_INV;
        const int cx = static_cast<int>(scatter_pos.x * inv_cell);
        const int cy = static_cast<int>(scatter_pos.y * inv_cell);
        const int cz = static_cast<int>(scatter_pos.z * inv_cell);
        unsigned int h = static_cast<unsigned int>(cx * 73856093) ^
                         static_cast<unsigned int>(cy * 19349663) ^
                         static_cast<unsigned int>(cz * 83492791);
        ser_hint = (h ^ (h >> 13)) & 0xFFu;
    }
    optixReorder(ser_hint, 8);
#endif
#else
    (void)scatter_pos;
    (void)result;
    (void)bounce;
#endif
}

// On escape (sample_scattering_event returned false), add the environment contribution carried by the
// path's throughput. The accounting follows the estimator:
//
// Analog free-flight (ADT, SDTracking §4.1): the event returns `false` with probability
// exp(-tau_total), so transmittance is baked into the sampling and the escape contribution is just
// throughput * env (no extra exp(-tau) -- multiplying again double-counts to exp(-2tau)*env). Matches
// volprim_prb.py:174-187, which contributes beta * emitter_val on escape.
//
// With NEE on, each scatter's env is already gathered via shadow rays, so a bounce>=1 escape would
// double-count; only the bounce-0 escape (camera ray straight to env, not covered by NEE) is added --
// and only when ENABLE_ANALYTIC_DIRECT did not already add the bounce-0 direct term elsewhere.
//
// With NEE off (pure analog), escape adds env at every bounce; under analytic-direct the bounce-0
// direct is added elsewhere, so only bounce>0 (indirect) is added here.
__device__ __forceinline__ void accumulate_escape_radiance(float3& radiance, float3 throughput,
                                                           const payloads::Miss& miss,
                                                           size_t bounce) {
    if constexpr (consts::ENABLE_NEE) {
        if constexpr (!consts::ENABLE_ANALYTIC_DIRECT) {
            if (bounce == 0) {
                radiance += throughput * miss.color();
            }
        }
    } else {
        if constexpr (consts::ENABLE_ANALYTIC_DIRECT) {
            if (bounce > 0) {
                radiance += throughput * miss.color();
            }
        } else {
            radiance += throughput * miss.color();
        }
    }
}

// Next-event estimation at a scatter vertex: add the direct-lighting contribution. Three estimators,
// selected by flag (wi is the incoming propagation direction, ray.direction_):
//   - runtime --ris : product-RIS (K env-IS candidates -> 1 reservoir -> 1 shadow ray);
//   - ENABLE_MIS    : balance-heuristic MIS over phase-IS (A) and env-IS (B), 2 shadow rays;
//   - else          : single-strategy phase-IS, 1 shadow ray.
// Empty when NEE is off (pure analog: env is gathered only on escape; see accumulate_escape_radiance).
__device__ __forceinline__ void accumulate_nee_direct(float3& radiance, float3 throughput,
                                                      float3 albedo,
                                                      const optix::ScatteringEvent<PrimsSet>& event,
                                                      float3 wi, random::PCG32& rng) {
    if constexpr (consts::ENABLE_NEE) {
        const auto base = throughput * albedo;

        if (launch_params.render_.use_ris_) {  // runtime --ris (default off = MIS)
            // ─── Product-RIS direct lighting: K env-IS candidates → 1 reservoir → 1 ray ───
            // Target p̂(ω) = phase(wi,ω)·lum(env(ω)) (the UNSHADOWED phase×env product that
            // neither phase-IS nor env-IS alone samples). Candidates drawn from env-IS
            // (proposal q = pdf_env), resampling weight w = p̂/q. A 1-survivor weighted
            // reservoir picks y ∝ w; ONLY y's transmittance is traced (1 shadow ray, vs 2
            // for balance-MIS). Unbiased RIS (Talbot 2005): with scalar target p̂ carrying
            // the RGB via f/p̂, ⟨L⟩ = base · env(y)/lum(env(y)) · T(y) · (Σ_k w_k / K).
            // K=1 collapses to the plain (unbiased) env-IS NEE estimator.
            const auto luma_w = make_float3(0.2126f, 0.7152f, 0.0722f);
            const int ris_k = static_cast<int>(launch_params.render_.ris_num_candidates_);
            float wsum = 0.0f;
            float3 y_dir = make_float3(0.0f, 0.0f, 0.0f);
            float3 y_env = make_float3(0.0f, 0.0f, 0.0f);
            for (int k = 0; k < ris_k; ++k) {
                const auto cand = env_is::sample(rng);
                if (cand.pdf <= 0.0f)
                    continue;
                const auto env_c = launch_params.env_map_.sample(cand.wo);
                const float lum_c = math::dot(env_c, luma_w);
                // w = p̂/q = phase(wi,ω)·lum(env(ω)) / pdf_env(ω)
                const float w = phase::eval(wi, cand.wo) * lum_c * math::rcp(cand.pdf);
                if (!(w > 0.0f))
                    continue;
                wsum += w;
                // Weighted reservoir (1 sample): keep candidate with prob w/wsum.
                if (random::sample_uniform(rng) * wsum < w) {
                    y_dir = cand.wo;
                    y_env = env_c;
                }
            }
            if (wsum > 0.0f) {
                const float lum_y = math::dot(y_env, luma_w);  // > 0 (w>0 ⇒ lum_c>0)
                if (lum_y > 0.0f) {
                    const auto T = compute_transmittance_to_env(event.position_, y_dir,
                                                                event.active_prims_);
                    const float W_ris = wsum * math::rcp(static_cast<float>(ris_k));
                    // f(y)/p̂(y) = env(y)·T/lum(env(y)); times the RIS normalization W_ris.
                    radiance += base * (y_env * math::rcp(lum_y)) * T * W_ris;
                }
            }
        } else if constexpr (consts::ENABLE_MIS) {
            // ─── Strategy A: phase importance sampling ───
            const auto a = phase::sample(wi, rng);
            const auto pdf_b_at_a = env_is::pdf(a.wo);
            const auto w_a = mis_balance(a.pdf, pdf_b_at_a);
            const auto env_a = launch_params.env_map_.sample(a.wo);
            const auto T_a = compute_transmittance_to_env(event.position_, a.wo,
                                                            event.active_prims_);
            // f / pdf_phase = phase * env * T / phase = env * T
            radiance += base * env_a * T_a * w_a;

            // ─── Strategy B: environment importance sampling ───
            const auto b = env_is::sample(rng);
            const auto phase_at_b = phase::eval(wi, b.wo);
            const auto w_b = mis_balance(b.pdf, phase_at_b);
            const auto env_b = launch_params.env_map_.sample(b.wo);
            const auto T_b = compute_transmittance_to_env(event.position_, b.wo,
                                                            event.active_prims_);
            // f / pdf_env = phase * env * T / pdf_env
            radiance += base * env_b * T_b * w_b * phase_at_b * math::rcp(b.pdf);
        } else {
            // Single-strategy NEE: phase IS only.
            // For phase IS, phase/pdf_phase == 1 → contribution = base · env · T.
            const auto sample = phase::sample(wi, rng);
            const auto env = launch_params.env_map_.sample(sample.wo);
            const auto T = compute_transmittance_to_env(event.position_, sample.wo,
                                                         event.active_prims_);
            radiance += base * env * T;
        }
    }
}

}  // namespace device
}  // namespace thesis
