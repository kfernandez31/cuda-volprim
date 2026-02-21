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
    const size_t pixel_linear_idx = launch_idx.y * launch_params.image_.width_ + launch_idx.x;

#ifdef THESIS_ENABLE_ADAPTIVE_SAMPLING
    // Adaptive sampling: check if pixel has already converged
    const auto sample_count = launch_params.image_.sample_counts_[pixel_linear_idx];

    // Check convergence only after sufficient samples for statistical validity
    if (sample_count >= consts::ADAPTIVE_MIN_SAMPLES) {
        const auto accumulator = launch_params.image_.accumulator_[pixel_linear_idx];
        const auto M2 = launch_params.image_.variance_[pixel_linear_idx];

        const auto mean_rgb = make_float3(accumulator) / static_cast<float>(sample_count);
        const auto variance = make_float3(M2) / static_cast<float>(math::max(sample_count - 1, 1UL));
        const auto std_dev = math::sqrt(variance);

        // Per-channel relative error (better than combined luminance approach)
        // Avoids one channel dominating the convergence criterion
        const auto per_channel_error = std_dev / math::max(mean_rgb, consts::ADAPTIVE_MIN_LUMINANCE);
        const auto max_relative_error = math::max(per_channel_error);

        if (max_relative_error < consts::ADAPTIVE_THRESHOLD) {
            return;  // Pixel converged, skip sampling
        }
    }
#endif  // THESIS_ENABLE_ADAPTIVE_SAMPLING

    // Accumulate radiance from batch_size samples in registers
    auto batch_radiance = make_float3(0.0f);

#ifdef THESIS_ENABLE_ADAPTIVE_SAMPLING
    // Pre-load variance state for per-sample updates
    const auto prev_count = launch_params.image_.sample_counts_[pixel_linear_idx];
    auto M2 = make_float3(launch_params.image_.variance_[pixel_linear_idx]);
#endif

    // Process batch_size samples per pixel
    for (size_t sample_in_batch = 0; sample_in_batch < launch_params.image_.batch_size_;
         ++sample_in_batch) {
        // Compute global sample index for this sample
        const size_t global_sample_idx = launch_params.image_.batch_offset_ + sample_in_batch;
        const size_t rng_seed = math::fma(
            pixel_linear_idx, launch_params.image_.num_samples_per_pixel_, global_sample_idx);

        // RNG setup (unique per sample)
        curandState rng;
        curand_init(launch_params.seed_, rng_seed, 0, &rng);

        // Ray setup with jittering
        const auto jitter = random::sample_uniform_2d(rng, 0.5f);
        auto ray = launch_params.camera_.jittered_ray(pixel_idx, jitter);

        auto throughput = make_float3(1.0f);
        auto radiance = make_float3(0.0f);

        optix::ScatteringEvent<consts::ACTIVE_PRIMS_CAPACITY> event;
        payloads::Miss miss;

        // Initialize active_prims from pre-computed camera containment (CPU-side, pre-sorted)
        event.active_prims_.init_from_presorted(launch_params.camera_active_prims_.data(),
                                                launch_params.camera_active_prims_.size());

        for (size_t bounce = 0; bounce < consts::MAX_BOUNCES; ++bounce) {
            const auto result = sample_scattering_event(ray, rng, event, miss);

            // no scattering - escaped medium
            if (!result) {
                auto tau = compute_optical_depth_along_ray(ray, event.active_prims_);
                radiance += (throughput * math::exp(-tau)) * miss.color();
                break;
            }

            // Evaluate albedo and environment lighting
            auto albedo = evaluate_albedo(event.position_, event.active_prims_);
            auto env = launch_params.env_map_.sample(event.direction_);
            radiance += throughput * albedo * env * consts::PHASE_VALUE;
            throughput *= albedo;

            // Russian Roulette
            if (bounce >= consts::RR_DEPTH) {
                auto p_survive = math::min(consts::RR_MAX_SURVIVAL, math::max(throughput));
                if (random::sample_uniform(rng) > p_survive) {
                    break;
                }
                throughput /= p_survive;
            }

            // Safety check: terminate path if throughput becomes non-finite due to numerical errors
            if (!isfinite(math::sum(throughput))) {
                break;
            }

            // Prepare next ray (direction is already unit from sample_phase)
            ray = geometry::Ray::spawn_unchecked(event.position_, event.direction_);
        }

        // Accumulate this sample's radiance into batch total
        batch_radiance += radiance;

#ifdef THESIS_ENABLE_ADAPTIVE_SAMPLING
        // Update variance per-sample using Welford's online algorithm
        // This is the CORRECT way: update variance for each individual sample, not batch sum
        const auto current_sample_idx = prev_count + sample_in_batch + 1;
        const auto prev_sample_idx = current_sample_idx - 1;

        // Old mean = accumulated radiance before this sample / previous count
        const auto old_mean = (prev_sample_idx > 0) ?
            (batch_radiance - radiance) / static_cast<float>(prev_sample_idx) :
            make_float3(0.0f);

        const auto delta1 = radiance - old_mean;
        const auto new_mean = batch_radiance / static_cast<float>(current_sample_idx);
        const auto delta2 = radiance - new_mean;
        M2 += delta1 * delta2;
#endif
    }  // End of batch loop}

    // Write accumulated radiance to accumulator buffer (no atomics needed, one thread per pixel)
    // Convert float3 to float4 and add to accumulator (W component remains 0.0f)
    launch_params.image_.accumulator_[pixel_linear_idx] += make_float4(batch_radiance);

#ifdef THESIS_ENABLE_ADAPTIVE_SAMPLING
    // Write back updated variance (M2 was updated per-sample in the loop above)
    launch_params.image_.variance_[pixel_linear_idx] = make_float4(M2);

    // Update sample count
    const auto new_count = prev_count + launch_params.image_.batch_size_;
    launch_params.image_.sample_counts_[pixel_linear_idx] = new_count;
#endif  // THESIS_ENABLE_ADAPTIVE_SAMPLING
}

}  // namespace device
}  // namespace thesis
