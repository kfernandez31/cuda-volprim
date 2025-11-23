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
#include <sutil/vec_math.h>

#include <assert.h>

namespace thesis {
namespace device {
namespace consts {

// TODO(kacper): select experimentally
constexpr size_t MAX_BOUNCES     = 128;  // Match Mitsuba production examples (64-128)
constexpr float  MIN_THROUGHPUT  = 1e-4;
constexpr size_t RR_DEPTH        = 3;
constexpr float  RR_MAX_SURVIVAL = 0.99; // Mitsuba uses 0.99 for bounce-level RR
constexpr float  PHASE_VALUE     = common::math::ONE_OVER_FOUR_PI_F; // 1 over unit sphere surface

} // namespace consts
} // namespace device
} // namespace thesis

extern "C" __global__ void __raygen__rg() {
    using namespace thesis::device;
    namespace math = thesis::common::math;

    const auto launch_idx = optixGetLaunchIndex();

    // RNG setup
    const auto global_sample_idx = launch_params.image_.getGlobalSampleIndex(launch_idx);
    curandState rng;
    curand_init(launch_params.seed_, global_sample_idx, 0, &rng);

    // Ray setup
    const auto pixel_idx = make_uint2(launch_idx.x, launch_idx.y);
    const auto jitter = random::sample_uniform_2d(rng, 0.5f);
    auto ray = launch_params.camera_.jittered_ray(pixel_idx, jitter);

    auto throughput = make_float3(1.0f);
    auto radiance = make_float3(0.0f);

    optix::ScatteringEvent<consts::MAX_CAPACITY> event;
    payloads::Miss miss;

    const auto is_debug = is_debug_thread();

    for (size_t bounce = 0; bounce < consts::MAX_BOUNCES; ++bounce) {
        if (is_debug) {
            printf("\n--- RAYGEN bounce %u ---\n", static_cast<uint>(bounce));
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
            radiance += (throughput * expf(-tau)) * miss.color();
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
            auto p_survive = fminf(consts::RR_MAX_SURVIVAL, math::max(throughput));
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
            bool first = true;
            for (auto prim : event.active_prims_) {
                if (!first) printf(",");
                printf("%u", prim);
                first = false;
            }
            printf("] size=%u\n", static_cast<uint>(event.active_prims_.size()));
        }
    }

    if (math::max(throughput) > consts::MIN_THROUGHPUT) {
        if (is_debug) printf("\nFinal ray contribution\n");
        auto tau = compute_optical_depth_along_ray(ray, event.active_prims_);
        auto env = launch_params.env_map_.sample(ray.direction_);
        if (is_debug) printf("Final tau=(%.3f,%.3f,%.3f), env=(%.3f,%.3f,%.3f)\n",
                                tau.x, tau.y, tau.z, env.x, env.y, env.z);
        radiance += throughput * expf(-tau) * env;
    }

    launch_params.image_[global_sample_idx] = radiance;
}
