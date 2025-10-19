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
constexpr size_t MAX_BOUNCES            = 1u;
constexpr size_t RUSSIAN_ROULETTE_DEPTH = 3u;
constexpr float  MIN_THROUGHPUT         = 1e-3f;
constexpr float  RR_MAX_SURVIVAL        = 0.99f;
constexpr float  PHASE_VALUE            = common::math::ONE_OVER_FOUR_PI_F; // 1 over unit sphere surface

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
        const auto hit = trace_ch(ray, 0.0f);
        if (!hit) {
            radiance = hit.unwrap_err().color(); // TODO(kacper): remove
        } else {
            auto idx = hit.unwrap().prim_idx;
            radiance = launch_params.primitives_[idx].albedo_;
        } 
    } else {
        /////// REMOVE ME vvv
        /*
        auto result_1 = trace_ch(ray, 0);
        if (!result_1) {
            const auto miss = result_1.unwrap_err();
            radiance = miss.color();
        } else {
            // hit 1
            const auto& hit_1 = result_1.unwrap();        
            const auto t_hit_1 = hit_1.t_hit;
            const auto prim_idx_1 = hit_1.prim_idx;

            const auto is_entry = !hit_1.is_exit;
            assert(is_entry);

            const auto& prim = launch_params.primitives_[prim_idx_1];

            // hit 2
            const auto result_2 = trace_ch(ray, t_hit_1);
            if (!result_2) {
                // only one hit - omit
                const auto miss = result_1.unwrap_err();
                radiance = miss.color();
            } else {
                const auto& hit_2 = result_2.unwrap();        
                const auto t_hit_2 = hit_2.t_hit;
                const auto prim_idx_2 = hit_2.prim_idx;

                const auto is_exit = !hit_1.is_exit;
                assert(is_exit);

                assert(t_hit_2 >= t_hit_1);
                if (t_hit_2 - t_hit_1 > 2) {
                    printf("t_hit_2 - t_hit_1 = %f\n", t_hit_2 - t_hit_1);
                    assert(t_hit_2 - t_hit_1 <= 2);
                }
                assert(prim_idx_1 == prim_idx_2);

                // Compute the optical depth (τ) and transmittance T = exp(-τ)
                const float3 density = prim.density_integral(ray, t_hit_1, t_hit_2);
                const float tau = length(density);               // approximate scalar τ
                const float T = expf(-tau);                      // transmittance

                // Environment radiance seen beyond second intersection
                const auto env_color = launch_params.env_map_.sample(ray.direction_);

                // Final radiance composition
                // - absorbed/colored part from medium itself
                // - transmitted background color
                radiance = prim.albedo_ * (1.0f - T) + env_color * T;
            }
        }
        */
        /////// REMOVE ME ^^^
    
        optix::ScatteringEvent<consts::MAX_PRIMS> event;
        payloads::Miss miss;

        for (size_t bounce = 0; bounce < consts::MAX_BOUNCES; ++bounce) {
            const auto result = sample_scattering_event(ray, rng, event, miss);

            // no scattering - escaped medium
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

        if (math::max(throughput) > 0.0f) {
            auto tau = compute_optical_depth_along_ray(ray);
            auto env = launch_params.env_map_.sample(ray.direction_);
            radiance += throughput * expf(-tau) * env;
        }
    }

    launch_params.image_[global_sample_idx] = radiance;
}
