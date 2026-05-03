#pragma once

#include "core/launch_params.cuh"
#include "core/random.cuh"
#include "core/sampling.cuh"

#include "thesis/common/utils/math.h"
#include "thesis/common/utils/types.h"
#include "thesis/device/utils/set.h"
#include "thesis/device/utils/vector.h"

#include <optix.h>
#include <vector_types.h>

#include <assert.h>

namespace thesis {
namespace device {

extern "C" __global__ void __raygen__rg() {
    using namespace thesis::device;
    namespace math = thesis::common::math;

    const auto launch_idx = optixGetLaunchIndex();
    const auto pixel_idx = make_uint2(launch_idx.x, launch_idx.y);
    // Widen one operand to size_t so width × height ≥ 2^32 (8K+) doesn't overflow uint32 arithmetic.
    const size_t pixel_linear_idx =
        static_cast<size_t>(launch_idx.y) * launch_params.image_.width_ + launch_idx.x;

    // Load Welford state once via read-only cache (written only at end of kernel, no aliasing)
    const auto prev_count = __ldg(&launch_params.image_.sample_counts_[pixel_linear_idx]);
    auto mean = make_float3(__ldg(&launch_params.image_.mean_[pixel_linear_idx]));

    // Variance (Welford M2) is only allocated when adaptive sampling is enabled.
    // Skip the read entirely when disabled — pointer is null, dereference would UB.
    auto M2 = make_float3(0.0f);
    if constexpr (consts::ENABLE_ADAPTIVE_SAMPLING) {
        M2 = make_float3(__ldg(&launch_params.image_.variance_[pixel_linear_idx]));
    }

    // AOV running means for the OptiX denoiser guide layers. Written only at bounce 0
    // of each path; carried across batches via the same per-pixel buffers. AOV buffers
    // are only allocated when --denoise is on, so guard reads with a null-pointer check.
    auto aov_albedo = make_float3(0.0f);
    auto aov_normal = make_float3(0.0f);
    if (launch_params.image_.albedo_aov_) {
        aov_albedo = make_float3(__ldg(&launch_params.image_.albedo_aov_[pixel_linear_idx]));
        aov_normal = make_float3(__ldg(&launch_params.image_.normal_aov_[pixel_linear_idx]));
    }

    // Adaptive sampling: check if pixel has already converged. Whole block is dead
    // code when ENABLE_ADAPTIVE_SAMPLING is false (M2 stays zero, threshold unreachable).
    if constexpr (consts::ENABLE_ADAPTIVE_SAMPLING) {
        if (prev_count >= consts::ADAPTIVE_MIN_SAMPLES) {
            const auto variance = M2 * math::rcp(static_cast<float>(prev_count - 1));
            const auto std_dev = math::sqrt(variance);

            // Per-channel relative error to avoid one channel dominating the criterion
            const auto safe_mean = make_float3(math::max(mean.x, consts::ADAPTIVE_MIN_LUMINANCE),
                                               math::max(mean.y, consts::ADAPTIVE_MIN_LUMINANCE),
                                               math::max(mean.z, consts::ADAPTIVE_MIN_LUMINANCE));
            const auto per_channel_error = std_dev / safe_mean;

            if (math::max(per_channel_error) < consts::ADAPTIVE_THRESHOLD) {
                return;  // Pixel converged, skip sampling
            }
        }
    }

    // Process batch_size samples per pixel
    for (size_t sample_in_batch = 0; sample_in_batch < launch_params.image_.batch_size_;
         ++sample_in_batch) {
        // Compute global sample index for this sample
        const size_t global_sample_idx = launch_params.image_.batch_offset_ + sample_in_batch;
        const size_t rng_seed = math::fma(
            pixel_linear_idx, launch_params.image_.num_samples_per_pixel_, global_sample_idx);

        // RNG setup (unique per sample)
        auto rng = random::init(launch_params.seed_, rng_seed);

        // Ray setup with jittering
        const auto jitter = random::sample_uniform_2d(rng, 0.5f);
        auto ray = launch_params.camera_.jittered_ray(pixel_idx, jitter);

        auto throughput = make_float3(1.0f);
        auto radiance = make_float3(0.0f);
        // Per-sample AOV captures, finalized at end of sample via Welford running mean.
        auto sample_aov_albedo = make_float3(0.0f);
        auto sample_aov_normal = make_float3(0.0f);

        optix::ScatteringEvent<PrimsSet> event;
        payloads::Miss miss;
        HitBuffer hit_buffer;

        // Initialize active_prims from pre-computed camera containment (CPU-side, pre-sorted)
        event.active_prims_.init_from_array(launch_params.camera_active_prims_.data(),
                                            launch_params.camera_active_prims_.size());

        for (size_t bounce = 0; bounce < consts::MAX_BOUNCES; ++bounce) {
            const auto result = sample_scattering_event(ray, rng, event, miss, hit_buffer);

            // First-bounce AOV capture. Normal: -ray.direction (camera-facing pseudo-normal,
            // standard for media without true geometry). Albedo: scatter-point albedo on
            // success, zero on escape (denoiser treats those regions as transparent).
            if (bounce == 0) {
                sample_aov_normal = -ray.direction_;
                if (result) {
                    sample_aov_albedo = evaluate_albedo(event.position_, event.active_prims_);
                }
            }

            // no scattering - escaped medium
            if (!result) {
                // With NEE on, the env contribution from each scatter is already
                // accumulated via shadow rays, so adding the escape for bounce >= 1
                // would double-count. The bounce == 0 escape (path traveled directly
                // from camera to env without scattering) is not covered by NEE and
                // must be added.
                if constexpr (consts::ENABLE_NEE) {
                    if (bounce == 0) {
                        const auto tau = event.escape_optical_depth_;
                        radiance += (throughput * math::exp(-tau)) * miss.color();
                    }
                } else {
                    const auto tau = event.escape_optical_depth_;
                    radiance += (throughput * math::exp(-tau)) * miss.color();
                }
                break;
            }

            // Evaluate albedo at the scatter point
            const auto albedo = evaluate_albedo(event.position_, event.active_prims_);

            if constexpr (consts::ENABLE_NEE) {
                const auto wi = ray.direction_;
                const auto base = throughput * albedo;

                if constexpr (consts::ENABLE_MIS) {
                    // ─── Strategy A: phase importance sampling ───
                    const auto a = phase::sample(wi, rng);
                    const auto pdf_b_at_a = env_is::pdf(a.wo);
                    const auto w_a = mis_balance(a.pdf, pdf_b_at_a);
                    const auto env_a = launch_params.env_map_.sample(a.wo);
                    const auto T_a = compute_transmittance_to_env(event.position_, a.wo,
                                                                    event.active_prims_,
                                                                    hit_buffer);
                    // f / pdf_phase = phase * env * T / phase = env * T
                    radiance += base * env_a * T_a * w_a;

                    // ─── Strategy B: environment importance sampling ───
                    const auto b = env_is::sample(rng);
                    const auto phase_at_b = phase::eval(wi, b.wo);
                    const auto w_b = mis_balance(b.pdf, phase_at_b);
                    const auto env_b = launch_params.env_map_.sample(b.wo);
                    const auto T_b = compute_transmittance_to_env(event.position_, b.wo,
                                                                    event.active_prims_,
                                                                    hit_buffer);
                    // f / pdf_env = phase * env * T / pdf_env
                    radiance += base * env_b * T_b * w_b * phase_at_b * math::rcp(b.pdf);
                } else {
                    // Single-strategy NEE: phase IS only.
                    // For phase IS, phase/pdf_phase == 1 → contribution = base · env · T.
                    const auto sample = phase::sample(wi, rng);
                    const auto env = launch_params.env_map_.sample(sample.wo);
                    const auto T = compute_transmittance_to_env(event.position_, sample.wo,
                                                                 event.active_prims_, hit_buffer);
                    radiance += base * env * T;
                }
            } else {
                // Unoccluded single-scatter (overestimates direct lighting in dense media —
                // ignores occlusion). For phase-IS-sampled directions phase/pdf_phase == 1,
                // so the contribution is throughput · albedo · env(ω) — no PHASE_VALUE factor.
                const auto env = launch_params.env_map_.sample(event.direction_);
                radiance += throughput * albedo * env;
            }
            throughput *= albedo;

            // Russian Roulette
            if (bounce >= consts::RR_DEPTH) {
                auto p_survive = math::min(consts::RR_MAX_SURVIVAL, math::max(throughput));
                if (random::sample_uniform(rng) > p_survive) {
                    break;
                }
                throughput /= p_survive;
            }

            // Early termination: throughput too low to contribute meaningfully
            if (math::max(throughput) < consts::MIN_THROUGHPUT) {
                break;
            }

            // Safety check: terminate path if throughput becomes non-finite due to numerical errors
            if (!isfinite(math::sum(throughput))) {
                break;
            }

            // Prepare next ray (direction is already unit from sample_phase)
            ray = geometry::Ray::spawn_unchecked(event.position_, event.direction_);
        }

        // Welford's online algorithm: numerically stable single-pass mean + M2.
        // M2 update is dead when ENABLE_ADAPTIVE_SAMPLING is false (compiler
        // strips it under the if constexpr below).
        const auto n_inv = math::rcp(static_cast<float>(prev_count + sample_in_batch + 1));
        const auto delta1 = radiance - mean;
        mean += delta1 * n_inv;
        if constexpr (consts::ENABLE_ADAPTIVE_SAMPLING) {
            const auto delta2 = radiance - mean;
            M2 += delta1 * delta2;
        }

        // AOVs: running mean only — variance not needed by the denoiser.
        if (launch_params.image_.albedo_aov_) {
            aov_albedo += (sample_aov_albedo - aov_albedo) * n_inv;
            aov_normal += (sample_aov_normal - aov_normal) * n_inv;
        }
    }  // End of batch loop

    // Write back Welford state
    if constexpr (consts::ENABLE_ADAPTIVE_SAMPLING) {
        launch_params.image_.variance_[pixel_linear_idx] = make_float4(M2);
    }
    launch_params.image_.mean_[pixel_linear_idx] = make_float4(mean);
    launch_params.image_.sample_counts_[pixel_linear_idx] = static_cast<uint16_t>(
        prev_count + launch_params.image_.batch_size_);
    if (launch_params.image_.albedo_aov_) {
        launch_params.image_.albedo_aov_[pixel_linear_idx] = make_float4(aov_albedo);
        launch_params.image_.normal_aov_[pixel_linear_idx] = make_float4(aov_normal);
    }
}

}  // namespace device
}  // namespace thesis
