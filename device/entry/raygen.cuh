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

    // Accumulate radiance from batch_size samples in registers
    auto batch_radiance = make_float3(0.0f);

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
    }  // End of batch loop}

    // Write accumulated radiance to accumulator buffer (no atomics needed, one thread per pixel)
    // Convert float3 to float4 and add to accumulator (W component remains 0.0f)
    launch_params.image_.accumulator_[pixel_linear_idx] += make_float4(batch_radiance);
}

}  // namespace device
}  // namespace thesis
