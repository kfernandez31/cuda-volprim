#pragma once

#include "core/constants.cuh"
#include "core/hit_record.cuh"
#include "core/launch_params.cuh"
#include "core/random.cuh"
#include "core/trace.cuh"

#include "thesis/common/geometry/intersection.h"
#include "thesis/common/utils/math.h"
#include "thesis/common/utils/preprocessor.h"
#include "thesis/common/utils/types.h"
#include "thesis/device/optix/scattering_event.h"
#include "thesis/device/payloads/miss.h"
#include "thesis/device/utils/bit_vector.h"
#include "thesis/device/utils/compact_set.h"
#include "thesis/device/utils/vector.h"

#include <optix.h>

#include <math.h>
#include <type_traits>

namespace thesis {
namespace device {

namespace math = ::thesis::common::math;

// PrimsSet: tracks which primitives are active (overlapping) at the current ray point.
// For small scenes (≤256 prims): BitVector — O(1) ops, indexed by primitive ID.
// For large scenes: CompactSet — O(k) ops, decoupled from scene size.
using PrimsSet = std::conditional_t<(consts::MAX_PRIMITIVES <= 256),
                                    utils::BitVector<((consts::MAX_PRIMITIVES + 63) & ~size_t{63})>,
                                    utils::CompactSet<prim_idx_t, consts::MAX_ACTIVE_PRIMS> >;
using HitBuffer = HitBufferSoA<consts::HIT_BUFFER_CAPACITY>;

// =============================================================================
// Phase function (Henyey-Greenstein, isotropic when consts::HG_G = 0)
// =============================================================================
// HG is the standard volumetric phase function: anisotropy parameter g ∈ (-1, 1)
// controls forward vs backward scattering. For HG, value(cos_θ) == pdf(cos_θ),
// so importance-sampling phase gives a unit phase/pdf ratio.
//
// References:
//   Henyey & Greenstein 1941 ("Diffuse radiation in the galaxy")
//   PBRT v4 §11.3.2

namespace phase {

// SIGN CONVENTION (WS2, FINDINGS §8.9): all call sites pass wi = ray.direction_ (the incoming
// propagation direction) to sample()/eval(). With that fixed wi convention, matching Mitsuba's
// `hg` phase function requires the anisotropy to enter the formula as **−HG_G**, not +HG_G:
// empirically CUDA(g)≡Mitsuba(−g) at same wi (18.7σ mismatch same-sign vs 1.4σ match opposite-
// sign). So eval_hg/sample use g_eff = −consts::HG_G, which makes the user-facing HG_G follow
// the standard/Mitsuba convention (HG_G>0 = forward scattering, as clouds need). NEE (raygen)
// and the continuation (sample_scattering_event) both use the same wi, so this single internal
// sign flip corrects both consistently and keeps sample↔eval↔pdf mutually consistent.
struct Sample {
    float3 wo;  // Outgoing direction (world space, unit length)
    float pdf;  // For HG, pdf == phase value
};

// HG phase value at cos_θ = wi · wo. Returns 1/(4π) when g is (runtime) isotropic.
// For g != 0: avoids a division by computing (1/denom)^(3/2) via rsqrt — single hardware
// instruction on NVIDIA, saves ~25 cycles vs the natural denom * sqrt(denom) form.
//
// g is now a RUNTIME parameter (launch_params.render_.hg_g_); the HG constants are pre-folded
// on the host (one±g², etc.) so this is the same FMA sequence the prior constexpr code used
// (unbiased, though not bit-exact under fast-math — see launch_params.h RenderParams note).
// eval uses +g (hg_g_), NOT g_eff. The sampling
// inversion (sample(), below) and this eval formula encode cosθ with OPPOSITE signs of the
// 2g·cosθ term, so to describe the SAME physical HG (mean cosθ = +g, forward), sample needs
// g_eff=−g while eval needs +g (FINDINGS §8.9/§8.10 — the MIS energy bug was a backward eval).
__device__ __forceinline__ float eval_hg(float cos_theta) {
    const auto& rp = launch_params.render_;
    if (rp.hg_isotropic_) {
        return consts::PHASE_VALUE;
    }
    // denom = (1 + g²) - 2g·cos_θ — single FMA (−2g·cos is exact: ×2 only shifts the exponent)
    const auto denom = math::fma(-2.0f * rp.hg_g_, cos_theta, rp.hg_one_plus_g2_);
    const auto inv_sqrt = math::rsqrt(denom);
    const auto inv_denom_3_2 = inv_sqrt * inv_sqrt * inv_sqrt;
    // hg_phase_coeff_ = PHASE_VALUE·(1-g²) pre-folded → single multiply (matches the old
    // constexpr fold; a micro-opt — not enough for full bit-exactness, see RenderParams note).
    return rp.hg_phase_coeff_ * inv_denom_3_2;
}

// Branchless stable orthonormal basis (Duff et al. 2017, "Building an ONB, Revisited").
__device__ __forceinline__ void onb_from_normal(float3 n, float3& tx, float3& ty) {
    const auto sign = math::copysign(1.0f, n.z);
    const auto a = -1.0f / (sign + n.z);
    const auto b = n.x * n.y * a;
    tx = make_float3(1.0f + sign * n.x * n.x * a, sign * b, -sign * n.x);
    ty = make_float3(b, sign + n.y * n.y * a, -n.y);
}

// Importance-sample HG phase. Returns wo with pdf = phase(wi · wo).
__device__ __forceinline__ Sample sample(float3 wi, random::PCG32& rng) {
    const auto u = random::sample_uniform_2d(rng);
    const auto& rp = launch_params.render_;

    float cos_theta;
    if (rp.hg_isotropic_) {
        cos_theta = math::fma(-2.0f, u.x, 1.0f);  // uniform on [-1, 1]
    } else {
        const auto g = rp.hg_g_eff_;  // = −g (Mitsuba sign convention; see top of namespace)
        // 1 - 2u and 1 + g(1 - 2u) — both single-FMA; HG constants pre-folded on the host
        const auto inner = math::fma(-2.0f, u.x, 1.0f);
        const auto denom = math::fma(g, inner, 1.0f);
        const auto sqr_term = rp.hg_one_minus_g2_ * math::rcp(denom);
        // (1 + g²) - sqr², then * (-1/(2g)) — hg_neg_inv_2g_ is nonzero in this branch.
        cos_theta = rp.hg_neg_inv_2g_ * math::fma(-sqr_term, sqr_term, rp.hg_one_plus_g2_);
    }

    const auto sin_theta = math::sqrt(math::max(0.0f, 1.0f - cos_theta * cos_theta));
    const auto phi = math::TWO_PI_F * u.y;

    // wo in local frame where wi is +z
    const auto wo_local =
        make_float3(sin_theta * math::cos(phi), sin_theta * math::sin(phi), cos_theta);

    float3 ex, ey;
    onb_from_normal(wi, ex, ey);
    const auto wo = wo_local.x * ex + wo_local.y * ey + wo_local.z * wi;

    return {wo, eval_hg(cos_theta)};
}

// Phase value at given (wi, wo). For HG, equals the sampling pdf.
__device__ __forceinline__ float eval(float3 wi, float3 wo) {
    return eval_hg(math::dot(wi, wo));
}

__device__ __forceinline__ float pdf(float3 wi, float3 wo) { return eval(wi, wo); }

}  // namespace phase

// =============================================================================
// Environment importance sampling
// =============================================================================
// Mitsuba-style 2D-CDF over the lat-long environment map (luminance × sin(θ)).
// CDFs are precomputed on the host (see EnvironmentMap::buildCdf) and live in
// launch_params.env_map_. Falls back to uniform-sphere when total_integral_ ≤ 0
// (e.g. a black env or a degenerate map), so a missing CDF is non-fatal.
//
// (u, v) ↔ direction convention is matched to env_map.sample() in
// device/params/environment_map.h:
//   u = atan2(dir.z, dir.x) / (2π) + 0.5      ↔  azimuth
//   v = acos(dir.y) / π                        ↔  polar from +y axis
// Solid-angle Jacobian: dω = 2π² · sin(θ) · du · dv.

namespace env_is {

struct Sample {
    float3 wo;
    float pdf;  // density in solid-angle measure (sr⁻¹)
};

// Smallest i in [0, n) such that cdf[i] > u. cdf is normalized to end at 1.
__device__ __forceinline__ int upper_bound(const float* cdf, int n, float u) {
    int lo = 0, hi = n;
    while (lo < hi) {
        const int mid = (lo + hi) >> 1;
        if (cdf[mid] <= u) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return math::min(lo, n - 1);
}

__device__ __forceinline__ Sample sample(random::PCG32& rng) {
    const auto& env = launch_params.env_map_;
    const auto u01 = random::sample_uniform_2d(rng);

    // Fallback: uniform sphere when no CDF is available.
    if (env.total_integral_ <= 0.0f || env.cdf_width_ == 0 || env.cdf_height_ == 0) {
        const auto z = math::fma(-2.0f, u01.x, 1.0f);
        const auto r = math::sqrt(math::max(0.0f, math::fma(-z, z, 1.0f)));
        const auto phi = math::TWO_PI_F * u01.y;
        return {make_float3(r * math::cos(phi), r * math::sin(phi), z), consts::PHASE_VALUE};
    }

    const int W = static_cast<int>(env.cdf_width_);
    const int H = static_cast<int>(env.cdf_height_);

    const int v = upper_bound(env.marginal_cdf_, H, u01.x);
    const int u = upper_bound(env.conditional_cdf_ + v * W, W, u01.y);

    // CONTINUOUS within-texel position (NOT the texel center). The CDF is piecewise-constant per
    // texel, so jittering uniformly inside the chosen texel leaves the pdf unchanged but makes
    // env-IS an unbiased *continuous* estimator. Returning the center instead biases any integrand
    // that varies within a texel (e.g. phase·T): MIS masks it (phase-IS covers the sub-texel
    // variation) but single-strategy env-IS / product-RIS exposes it (furnace −0.7%).
    const auto jit = random::sample_uniform_2d(rng);
    const float u_norm = (static_cast<float>(u) + jit.x) * math::rcp(static_cast<float>(W));
    const float v_norm = (static_cast<float>(v) + jit.y) * math::rcp(static_cast<float>(H));

    const float theta = math::fma(2.0f * math::PI_F, u_norm, -math::PI_F);  // azimuth ∈ [-π, π]
    const float polar = v_norm * math::PI_F;                                 // polar ∈ [0, π]
    const float sin_polar = math::sin(polar);

    const auto wo = make_float3(sin_polar * math::cos(theta), math::cos(polar),
                                 sin_polar * math::sin(theta));

    // pdf in (u, v) space (normalized to integrate to 1 over [0,1]²):
    //   p_uv = (joint_density / total_integral) · W · H
    // Convert to solid angle: p_ω = p_uv / (2π² · sin(polar))
    const float joint = env.joint_density_[v * W + u];
    const float p_uv = (joint * math::rcp(env.total_integral_)) * static_cast<float>(W * H);
    const float jacobian = 2.0f * math::PI_F * math::PI_F * sin_polar;
    const float pdf_omega = (sin_polar > 0.0f) ? p_uv * math::rcp(jacobian) : 0.0f;

    return {wo, pdf_omega};
}

__device__ __forceinline__ float pdf(float3 wo) {
    const auto& env = launch_params.env_map_;
    if (env.total_integral_ <= 0.0f || env.cdf_width_ == 0 || env.cdf_height_ == 0) {
        return consts::PHASE_VALUE;
    }

    const int W = static_cast<int>(env.cdf_width_);
    const int H = static_cast<int>(env.cdf_height_);

    // Direction → (u_norm, v_norm), matching env_map.sample()
    const float theta = atan2f(wo.z, wo.x);
    const float polar = acosf(math::clamp(wo.y, -1.0f, 1.0f));
    const float u_norm = math::fma(theta, math::ONE_OVER_TWO_PI_F, 0.5f);
    const float v_norm = polar * math::ONE_OVER_PI_F;

    int u = static_cast<int>(u_norm * static_cast<float>(W));
    int v = static_cast<int>(v_norm * static_cast<float>(H));
    u = u < 0 ? 0 : (u >= W ? W - 1 : u);
    v = v < 0 ? 0 : (v >= H ? H - 1 : v);

    const float joint = env.joint_density_[v * W + u];
    const float sin_polar = math::sin(polar);
    if (joint <= 0.0f || sin_polar <= 0.0f) {
        return 0.0f;
    }
    const float p_uv = (joint * math::rcp(env.total_integral_)) * static_cast<float>(W * H);
    const float jacobian = 2.0f * math::PI_F * math::PI_F * sin_polar;
    return p_uv * math::rcp(jacobian);
}

}  // namespace env_is

// MIS balance heuristic. Returns p_a / (p_a + p_b). Power heuristic (β=2) is slightly
// better near distribution boundaries but the balance heuristic is sufficient and cheaper.
__device__ __forceinline__ float mis_balance(float pdf_a, float pdf_b) {
    const auto sum = pdf_a + pdf_b;
    return (sum > 0.0f) ? pdf_a / sum : 0.0f;
}

__device__ __forceinline__ float3 evaluate_albedo(float3 pos, const PrimsSet& prims) {
    auto accum_albedo = make_float3(0.0f);
    auto accum_weight = 0.0f;

    for (auto idx : prims) {
        const auto& prim = launch_params.primitives_[idx];

        const auto sigma_t = prim.optical_thickness_;  // extinction coefficient
        const auto albedo = prim.albedo_;
        const auto pdf = prim.pdf(pos);  // density at pos

        const auto weight = sigma_t * pdf;
        accum_albedo += albedo * weight;
        accum_weight += weight;
    }

    // Empty active_prims or vanishing density: return zero to avoid 0/0 = NaN
    // propagating through the path. Can happen at grazing angles where
    // final_active_prims is rebuilt without survivors; rare but real.
    if (accum_weight <= 0.0f) {
#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
        printf("ERROR: evaluate_albedo zero total weight (empty active_prims?)\n");
#endif
        return make_float3(0.0f);
    }
    return accum_albedo * math::rcp(accum_weight);
}

// Helper: Collect ray-primitive entry hits (exits computed lazily)
// Clears and fills the provided hit buffer. Also returns Miss payload if provided.
__device__ void collect_hits(const geometry::Ray& ray, HitBuffer& hit_buffer,
                             payloads::Miss* out_miss = nullptr) {
    hit_buffer.clear();

    // STEP 1: Trace with backface culling to get entry hits (no exits!)
    auto miss = trace_ch_collect(ray, 0.0f, consts::INF_F, hit_buffer);
    if (out_miss)
        *out_miss = miss;  // Optionally return Miss payload

    // No exit computation — exits are computed on-demand in the argmin loop.
    // No sorting — argmin doesn't need sorted hits.

#ifdef DEBUG
    if (hit_buffer.full()) {
        printf("WARNING: Hit buffer overflow (%zu/%zu entries) — ray may be biased\n",
               hit_buffer.size(), hit_buffer.capacity());
    }
#endif
}

// Per-primitive analog free-flight optical-depth threshold: τ = -log(1-χ), χ ~ U(0,1). The min
// over independent per-primitive τ samples is the ADT collision estimator (SDTracking §4.1).
// Factored out of the two argmin loops below so the draw is defined once.
__device__ __forceinline__ float sample_free_flight_tau(random::PCG32& rng) {
    const float chi = random::sample_uniform(rng);
    return -math::log(math::max(1.0f - chi, 1e-10f));
}

// Sample scattering event using argmin approach (no sorting!)
// Based on Analog Decomposition Tracking theorem from SDTracking paper (Section 4.1):
// The minimum of independent inverse CDFs gives the same distribution as sorting
// `out_origin_inside` (optional): if non-null, receives the ray-origin-inside primitive
// set built by the initial scan below, BEFORE it is consumed/rebuilt for the scatter
// point. The caller (raygen, bounce 0) reuses it for the analytic-direct transmittance
// instead of re-running the same O(N) point-inside scan over all primitives.
// `first_bounce`: when true, the origin-inside primitive set is built fresh by an O(N)
// point-in-bound scan over all primitives (needed for the camera ray). When false, the
// caller guarantees `event.active_prims_` ALREADY holds the origin-inside set — because
// the previous bounce left it equal to the scatter point's active set, and the new ray
// starts AT that scatter point, so its origin-inside set is identical (any prim the new
// origin is inside was, by construction, active at the previous scatter). Skipping the
// scan for bounce>0 removes the dominant repeated O(N) cost. See FINDINGS.
__device__ __noinline__ bool sample_scattering_event(const geometry::Ray& ray, random::PCG32& rng,
                                                     optix::ScatteringEvent<PrimsSet>& event,
                                                     payloads::Miss& miss, HitBuffer& hit_buffer,
                                                     bool first_bounce,
                                                     PrimsSet* out_origin_inside = nullptr) {
    auto& active_prims = event.active_prims_;

    const size_t num_primitives = launch_params.primitives_.size();

    if (first_bounce) {
        // Camera ray: no prior active set to inherit — scan all primitives.
        active_prims.clear();
        uint32_t origin_overlap = 0;
        for (size_t i = 0; i < num_primitives; ++i) {
            const auto& prim = launch_params.primitives_[i];
            if (common::geometry::point_inside_bvh_bound(ray.origin_, prim)) {
                ++origin_overlap;
                if (!active_prims.insert(static_cast<prim_idx_t>(i)))
                    report_overflow(OVERFLOW_ACTIVE_SET);
            }
        }
        if (launch_params.render_.measure_caps_) {
            atomicMax(&launch_params.measure_buf_[MEASURE_ACTIVE_MAX], origin_overlap);
        }
    }
    // else: active_prims already == origin-inside set (inherited from the previous bounce's
    // scatter-point active set). No scan needed — this is the whole point of the optimization.

    // Hand the origin-inside set to the caller before it is modified below (the argmin
    // path clears it on escape and rebuilds it at the scatter point). Lets the caller
    // skip a duplicate full-scene scan for the same origin.
    if (out_origin_inside != nullptr) {
        *out_origin_inside = active_prims;
    }

    collect_hits(ray, hit_buffer, &miss);
    if (launch_params.render_.measure_caps_) {
        atomicMax(&launch_params.measure_buf_[MEASURE_HIT_MAX], hit_buffer.total_seen_);
    }

    if (hit_buffer.empty() && active_prims.empty()) {
        // No primitives along the ray — escape with unit transmittance. Analog
        // free-flight: the escape contribution is just env, no τ factor needed.
        return false;
    }

    float t_scatter_min = consts::INF_F;

    for (auto prim_idx : active_prims) {
        const auto& prim = launch_params.primitives_[prim_idx];

        // Ray origin is inside the primitive's 3σ BVH bound (active_prims invariant),
        // so use exit_from_inside; compute_exit_from_entry assumes entry-on-surface.
        const float t_exit = common::geometry::exit_from_inside(ray, prim);

        // Independent per-primitive free-flight threshold (ADT requirement).
        const float tau_i = sample_free_flight_tau(rng);
        const float t_scatter = prim.inv_cdf(ray, tau_i);

        if (t_scatter >= 0.0f && t_scatter < t_scatter_min && t_scatter <= t_exit) {
            t_scatter_min = t_scatter;
        }
    }

    for (size_t j = 0; j < hit_buffer.size(); ++j) {
        const float hit_t = hit_buffer.t_hit_[j];
        const auto& prim = launch_params.primitives_[hit_buffer.prim_idx_[j]];

        // Independent per-primitive free-flight threshold (ADT requirement).
        const float tau_j = sample_free_flight_tau(rng);

        // Span-restricted CDF: solves optical_depth(ray, hit_t, t_scatter) = tau_j.
        // Replaces the prior full-Gaussian inv_cdf + reject (t_scatter >= hit.t_hit),
        // which was biased — rejected samples were dropped rather than re-rolled.
        float t_scatter = prim.inv_cdf_span(ray, hit_t, tau_j);

        // Clamp FP undershoot: the true segment-CDF inverse is >= hit_t by construction
        // (optical depth from hit_t to hit_t is 0), but the erf/erfinv round-trip can
        // land a few ULPs below it (χ≈0 → τ≈0 → true solution == hit_t exactly). A
        // sub-entry winner is then EXCLUDED from its own scatter's active set by the
        // rebuild filter below (`hit_t > t_scatter_min → skip`), zeroing the albedo and
        // silently killing the path. Mechanism proven at bit level during the cap-free
        // streaming campaign (capfree_b_gate.md, branch feature/cap-free-streaming).
        // NaN/±inf saturation values fail the `>= 0` guard below and stay rejected.
        if (t_scatter >= 0.0f && t_scatter < hit_t) {
            t_scatter = hit_t;
        }

        // Guard t_scatter >= 0 (mirrors the active-prims loop above). A degenerate primitive
        // can make inv_cdf_span saturate erfinv(±1) → ±inf / negative; without this guard a
        // -inf t_scatter passes (-inf < t_scatter_min) and then (-inf <= t_exit), setting
        // t_scatter_min = -inf → scatter position = ray.at(-inf) = ±inf → NaN albedo/radiance
        // (the dense-asset NaN, FINDINGS §8.x). `>= 0` rejects negative, -inf AND NaN cleanly;
        // +inf is already rejected by `< t_scatter_min`. An invalid distance is correctly no scatter.
        if (t_scatter >= 0.0f && t_scatter < t_scatter_min) {
            const auto w = prim.transform_dir_local(ray.direction_);
            const auto w_len2 = math::length2(w);
            const float t_exit =
                common::geometry::compute_exit_from_entry(ray, hit_t, prim, w_len2);

            if (t_scatter <= t_exit) {
                t_scatter_min = t_scatter;
            }
        }
    }

    if (t_scatter_min >= consts::INF_F) {
        // Escape: no per-primitive free-flight resolved within its exit. ADT-min
        // already accounts for the escape probability exp(-τ_total) — caller's
        // contribution is just env. No need to integrate τ explicitly here.
        active_prims.clear();
        return false;
    }

    // Rebuild active_prims at the scatter point
    PrimsSet final_active_prims;

    // Recompute exits for primitives containing the ray origin.
    // Origin-inside ⇒ exit_from_inside; compute_exit_from_entry assumes entry-on-surface.
    for (auto prim_idx : active_prims) {
        const auto& prim = launch_params.primitives_[prim_idx];
        const float t_exit = common::geometry::exit_from_inside(ray, prim);
        if (t_scatter_min <= t_exit) {
            if (!final_active_prims.insert(prim_idx))
                report_overflow(OVERFLOW_ACTIVE_SET);
        }
    }

    for (size_t j = 0; j < hit_buffer.size(); ++j) {
        const float hit_t = hit_buffer.t_hit_[j];
        if (hit_t > t_scatter_min)
            continue;  // Skip hits after scatter point

        const auto hit_prim_idx = hit_buffer.prim_idx_[j];
        const auto& prim = launch_params.primitives_[hit_prim_idx];
        const auto w = prim.transform_dir_local(ray.direction_);
        const auto w_len2 = math::length2(w);
        const float t_exit =
            common::geometry::compute_exit_from_entry(ray, hit_t, prim, w_len2);

        if (t_scatter_min <= t_exit) {
            if (!final_active_prims.insert(hit_prim_idx))
                report_overflow(OVERFLOW_ACTIVE_SET);
        }
    }

    active_prims = final_active_prims;

    // --measure-caps: true point-overlap at the scatter vertex, counted by the same
    // containment predicate the bounce-0 scan uses — NOT final_active_prims.size(),
    // which is clipped by the compiled MAX_ACTIVE_PRIMS. O(N) per scatter, gated.
    if (launch_params.render_.measure_caps_) {
        const float3 scatter_pos = ray.at(t_scatter_min);
        uint32_t overlap = 0;
        for (size_t i = 0; i < num_primitives; ++i) {
            if (common::geometry::point_inside_bvh_bound(scatter_pos,
                                                         launch_params.primitives_[i]))
                ++overlap;
        }
        atomicMax(&launch_params.measure_buf_[MEASURE_ACTIVE_MAX], overlap);
    }

    // Set the scattering event. Phase sampling uses the incoming ray direction so HG
    // (consts::HG_G != 0) produces correct forward/back-scattering. With g = 0 it reduces
    // to uniform-sphere isotropic.
    event.t_hit_ = t_scatter_min;
    event.position_ = ray.at(t_scatter_min);
    event.direction_ = phase::sample(ray.direction_, rng).wo;

    return true;
}

// Compute scalar transmittance exp(-τ) along a shadow ray from `origin` in `direction`.
// `active_prims` are the primitives the scatter point is inside; their exits are
// integrated here (those prims report no forward entry hit, so the anyhit cannot
// see them). Every primitive the shadow ray ENTERS is integrated inline by the
// transmittance-mode anyhit during a single GAS descent (no HitBuffer needed).
// Marked __noinline__ to keep its register footprint out of the scatter path.
__device__ __noinline__ float compute_transmittance_to_env(float3 origin, float3 direction,
                                                            const PrimsSet& active_prims) {
    const auto shadow_ray = geometry::Ray::spawn_unchecked(origin, direction);

    // Transmittance needs only the TOTAL optical depth along the ray. Optical depth is
    // ADDITIVE across primitives (densities sum in overlaps), so each primitive can be
    // integrated ONCE over its full [entry, exit] span. The previous implementation
    // mirrored the primary-ray escape path: build an event list, sort it, and march
    // segment-by-segment summing optical_depth over all active prims per segment — that
    // is O(events × active_prims) ≈ O(A²) erf evaluations and was ~85% of the whole frame
    // (NEE fires this per scatter, ×2 under MIS). The segmentation is only needed to find
    // a scatter DISTANCE on the primary ray; for a shadow ray it is pure waste. This is
    // O(A): one optical_depth per primitive. Result is identical up to float summation
    // order. (See WAVEFRONT_PLAN.md time-split / FINDINGS.)
    float acc_tau = 0.0f;

    // Primitives containing the origin: integrate from 0 to their exit (origin is inside
    // the 3σ bound, so exit_from_inside — the full quadratic).
    for (auto prim_idx : active_prims) {
        const auto& prim = launch_params.primitives_[prim_idx];
        const float t_exit = common::geometry::exit_from_inside(shadow_ray, prim);
        if (t_exit > 0.0f && t_exit < consts::INF_F) {
            acc_tau += prim.optical_depth(shadow_ray, 0.0f, t_exit);
            if (acc_tau >= consts::MAX_OPTICAL_DEPTH) return 0.0f;  // exp(-τ) underflows
        }
    }

    // Primitives the shadow ray ENTERS: integrated inline by the transmittance-mode
    // anyhit during a single GAS descent (fused — no HitBuffer round-trip). Optical
    // depth is additive across primitives, so the inline accumulation order is
    // irrelevant and the result is identical to the buffered loop up to float
    // summation order.
    acc_tau += trace_transmittance(shadow_ray, 0.0f, consts::INF_F);

    return math::exp(-acc_tau);
}

}  // namespace device
}  // namespace thesis
