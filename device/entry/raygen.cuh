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

namespace thesis {
namespace device {
namespace consts {

// TODO(kacper): select experimentally
constexpr auto MAX_BOUNCES         = 500u;
constexpr auto RUSSIAN_ROULETTE_DEPTH = 3u;
constexpr auto MIN_THROUGHPUT      = 1e-3f;
constexpr auto RR_MAX_SURVIVAL     = 0.99f;
constexpr auto PHASE_VALUE = common::math::ONE_OVER_FOUR_PI_F; // 1 over unit sphere surface

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

    if (launch_params.debug_) {
        // Debug output for center pixel
        if (pixel_idx.x == launch_params.image_.width() / 2 && 
            pixel_idx.y == launch_params.image_.height() / 2) {
            float dir_len = sqrtf(ray.direction_.x*ray.direction_.x + 
                                 ray.direction_.y*ray.direction_.y + 
                                 ray.direction_.z*ray.direction_.z);
            printf("RAYGEN: Ray origin=(%.3f,%.3f,%.3f) dir=(%.3f,%.3f,%.3f) dir_length=%.6f\n",
                   ray.origin_.x, ray.origin_.y, ray.origin_.z,
                   ray.direction_.x, ray.direction_.y, ray.direction_.z,
                   dir_len);
            printf("RAYGEN: Expected sphere hit range: t=[%.3f, %.3f]\n", 0.0f, 2.0f);
        }
        
        const auto hit = trace_ch(ray, -0.001f);  // Allow small negative t for rays starting on/near sphere surface
        if (hit) {
            auto idx = hit.unwrap().prim_idx;
            radiance = launch_params.primitives_[idx].albedo_;
        } else {
            radiance = hit.unwrap_err().color();
        }
    } else {
        optix::ScatteringEvent<consts::MAX_PRIMS> event;
        payloads::Miss miss;

        for (size_t bounce = 0; bounce < consts::MAX_BOUNCES; ++bounce) {
            const auto result = sample_scattering_event(ray, rng, event, miss);

            // no scattering - escaped mediums
            if (!result) {
                auto tau = compute_optical_depth_along_ray(ray);
                radiance += (throughput * expf(-tau)) * miss.color();
                break;
            }

            // Evaluate albedo and environment lighting
            auto albedo = evaluate_albedo(event.position_, event.active_prims_);
            auto env = launch_params.env_map_.sample(event.direction_);
            radiance += throughput * albedo * env * consts::PHASE_VALUE;

            // Update energy by scattered amount (albedo)
            throughput *= albedo;

            // Russian Roulette
            if (bounce >= consts::RUSSIAN_ROULETTE_DEPTH) {
                float p_survive = fminf(consts::RR_MAX_SURVIVAL, math::max(throughput));
                if (random::sample_uniform(rng) > p_survive) {
                    break;
                }
                throughput /= p_survive;
            }

            // TODO(kacper): I believe the first condition is unreachable
            if (!isfinite(math::sum(throughput)) || math::max(throughput) < consts::MIN_THROUGHPUT) {
                break;
            }

            // Prepare next ray
            ray = geometry::Ray::spawn(event.position_, event.direction_);

            // Clear active prims
            event.active_prims_.clear();
        }
    }
    
    launch_params.image_[global_sample_idx] = radiance;
}
