#pragma once

#include "core/launch_params.cuh"
#include "core/sampling.cuh"
#include "core/random.cuh"

#include "thesis/device/utils/vector.h"
#include "thesis/device/utils/set.h"
#include "thesis/common/utils/math.h"
#include "thesis/common/utils/types.h"

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

    const auto is_debug = is_debug_thread();

    // TODO: remove - diagnostic print to trace execution
    if (is_debug) {
        printf("RAYGEN: Starting pixel (%u,%u), batch_size=%u\n",
               pixel_idx.x, pixel_idx.y,
               static_cast<uint>(launch_params.image_.batch_size_));
    }

    // Accumulate radiance from batch_size samples in registers
    auto batch_radiance = make_float3(0.0f);

    // Process batch_size samples per pixel
    for (size_t sample_in_batch = 0; sample_in_batch < launch_params.image_.batch_size_; ++sample_in_batch) {
        // TODO: remove - diagnostic for batch processing
        if (is_debug) {
            printf("\n=== STARTING SAMPLE %u/%u ===\n",
                   static_cast<uint>(sample_in_batch),
                   static_cast<uint>(launch_params.image_.batch_size_));
        }

        // Compute global sample index for this sample
        const size_t global_sample_idx = launch_params.image_.batch_offset_ + sample_in_batch;
        const size_t rng_seed = math::fma(pixel_linear_idx, launch_params.image_.num_samples_per_pixel_, global_sample_idx);

        // RNG setup (unique per sample)
        if (is_debug) {
            printf("Initializing RNG with seed=%u, rng_seed=%lu\n",
                   launch_params.seed_, rng_seed);
        }
        curandState rng;
        curand_init(launch_params.seed_, rng_seed, 0, &rng);
        if (is_debug) {
            printf("RNG initialized successfully\n");
        }

        // Ray setup with jittering
        const auto jitter = random::sample_uniform_2d(rng, 0.5f);
        auto ray = launch_params.camera_.jittered_ray(pixel_idx, jitter);

        auto throughput = make_float3(1.0f);
        auto radiance = make_float3(0.0f);

        optix::ScatteringEvent<consts::MAX_CAPACITY> event;
        payloads::Miss miss;

        // Initialize active_prims from pre-computed camera containment (CPU-side)
        for (uint i = 0; i < launch_params.camera_active_prims_.size(); ++i) {
            event.active_prims_.insert(launch_params.camera_active_prims_[i]); // TODO: this might be an avoidable copy
        }

        if (is_debug && !event.active_prims_.empty()) {
            printf("\nCamera inside prims: [");
            const auto size = event.active_prims_.size();
            for (size_t i = 0; i < size; i++) {
                if (i > 0) printf(",");
                printf("%u", event.active_prims_[i]);
            }
            printf("] size=%u\n", static_cast<uint>(size));
        }

        for (size_t bounce = 0; bounce < consts::MAX_BOUNCES; ++bounce) {
            if (is_debug) {
                printf("\n--- RAYGEN bounce %u/%u ---\n", static_cast<uint>(bounce), consts::MAX_BOUNCES);
                printf("Ray: origin=(%.3f,%.3f,%.3f), dir=(%.3f,%.3f,%.3f)\n",
                        ray.origin_.x, ray.origin_.y, ray.origin_.z,
                        ray.direction_.x, ray.direction_.y, ray.direction_.z);
            }

            const auto result = sample_scattering_event(ray, rng, event, miss);

            // no scattering - escaped medium
            if (!result) {
                if (is_debug) printf("No scattering, computing final transmittance\n");
                auto tau = compute_optical_depth_along_ray(ray, event.active_prims_);
                if (is_debug) printf("Final tau=(%.3f,%.3f,%.3f)\n", tau.x, tau.y, tau.z);
                radiance += (throughput * math::exp(-tau)) * miss.color();
                break;
            }

            if (is_debug) printf("Scattering occurred, evaluating albedo\n");

            // Evaluate albedo and environment lighting
            auto albedo = evaluate_albedo(event.position_, event.active_prims_);

            auto env = launch_params.env_map_.sample(event.direction_);

            if (is_debug) printf("Albedo=(%.3f,%.3f,%.3f), env=(%.3f,%.3f,%.3f)\n",
                                    albedo.x, albedo.y, albedo.z, env.x, env.y, env.z);

            radiance += throughput * albedo * env * consts::PHASE_VALUE;

            // Update energy by scattered amount (albedo)
            throughput *= albedo;

            // Russian Roulette
            if (bounce >= consts::RR_DEPTH) {
                auto p_survive = math::min(consts::RR_MAX_SURVIVAL, math::max(throughput));
                if (random::sample_uniform(rng) > p_survive) {
                    if (is_debug) printf("Russian roulette terminated\n");
                    break;
                }
                throughput /= p_survive;
            }

            // Safety check: terminate path if throughput becomes non-finite due to numerical errors
            if (!isfinite(math::sum(throughput))) {
                if (is_debug) printf("ERROR: Non-finite throughput detected, terminating path\n");
                break;
            }

            // Prepare next ray (direction is already unit from sample_phase)
            ray = geometry::Ray::spawn_unchecked(event.position_, event.direction_);

            if (is_debug) {
                printf("Spawned new ray at scattering point\n");
                printf("New ray: origin=(%.3f,%.3f,%.3f), dir=(%.3f,%.3f,%.3f)\n",
                        ray.origin_.x, ray.origin_.y, ray.origin_.z,
                        ray.direction_.x, ray.direction_.y, ray.direction_.z);
                printf("Preserving active_prims for next bounce: [");
                const auto size = event.active_prims_.size();
                for (size_t i = 0; i < size; i++) {
                    if (i > 0) printf(",");
                    printf("%u", event.active_prims_[i]);
                }
                printf("] size=%u\n", static_cast<uint>(size));
            }
        }

        // TODO: remove - diagnostic to check if we hit max bounces
        if (is_debug) {
            printf("Exited bounce loop after processing samples\n");
        }

        if (math::max(throughput) > consts::MIN_THROUGHPUT) {
            if (is_debug) printf("\nFinal ray contribution\n");
            auto tau = compute_optical_depth_along_ray(ray, event.active_prims_);
            auto env = launch_params.env_map_.sample(ray.direction_);
            if (is_debug) printf("Final tau=(%.3f,%.3f,%.3f), env=(%.3f,%.3f,%.3f)\n",
                                    tau.x, tau.y, tau.z, env.x, env.y, env.z);
            radiance += throughput * math::exp(-tau) * env;
        }

        // Accumulate this sample's radiance into batch total
        batch_radiance += radiance;

        // TODO: remove - diagnostic for batch processing
        if (is_debug) {
            printf("=== FINISHED SAMPLE %u ===\n\n",
                   static_cast<uint>(sample_in_batch));
        }
    }  // End of batch loop

    // TODO: remove - diagnostic to check if we exit batch loop
    if (is_debug) {
        printf("Exited batch loop, writing to accumulator at pixel_linear_idx=%lu\n", pixel_linear_idx);
    }

    // Write accumulated radiance to accumulator buffer (no atomics needed, one thread per pixel)
    launch_params.image_.accumulator_[pixel_linear_idx] += batch_radiance;

    if (is_debug) {
        printf("Successfully wrote to accumulator\n");
    }
}

}  // namespace device
}  // namespace thesis
