#pragma once

#include "device/core/launch_params.cuh"
#include "device/core/sampling.cuh"
#include "device/core/random.cuh"

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
constexpr auto MAX_BOUNCES         = 64u;
constexpr auto RUSSIAN_ROULETTE_DEPTH = 3u;
constexpr auto MIN_THROUGHPUT      = 1e-3f;
constexpr auto RR_MAX_SURVIVAL     = 0.99f;

} // namespace consts
} // namespace device
} // namespace thesis

extern "C" __global__ void __raygen__rg() {
    const auto launch_idx = optixGetLaunchIndex();
    const auto pixel = make_uint2(launch_idx.x, launch_idx.y);
    const auto sample_idx = launch_idx.z;

    // initialize rng
    curandState rng;
    curand_init(params.seed_, pixel.y * params.image_.width_ + pixel.x, sample_idx, &rng);

    const auto jitter = thesis::device::random::sample_uniform_2d(&rng, 0.5f);
    auto ray = params.camera_.jittered_ray(pixel, jitter);

    auto throughput = make_float3(1.0f);
    auto radiance = make_float3(0.0f);

    for (size_t bounce = 0; bounce < thesis::device::consts::MAX_BOUNCES; ++bounce) {
        auto evt = thesis::device::sample_scattering_event(ray, &rng);

        // no scattering - escaped mediums
        if (!evt) {
            auto tau = thesis::device::compute_optical_depth_along_ray(ray);
            auto env = params.env_map_.sample(ray.direction_);
            radiance += throughput * expf(-tau) * env;
            break;
        }

        // Evaluate albedo and environment lighting
        auto albedo = thesis::device::evaluate_albedo(evt->position_, evt->active_prims_);
        auto env = params.env_map_.sample(evt->direction_);
        radiance += throughput * albedo * env * thesis::device::consts::PHASE_VALUE;

        // Update energy by scattered amount (albedo)
        throughput *= albedo;

        // Russian Roulette
        if (bounce >= thesis::device::consts::RUSSIAN_ROULETTE_DEPTH) {
            float p_survive = fminf(thesis::device::consts::RR_MAX_SURVIVAL, thesis::math::max(throughput));
            if (thesis::device::random::sample_uniform(&rng) > p_survive) {
                break;
            }
            throughput /= p_survive;
        }

        // Prepare next ray
        ray = thesis::device::Ray::spawn(evt->position_, evt->direction_);

        // TODO(kacper): I believe the first condition is unreachable
        if (!isfinite(thesis::math::sum(throughput)) || thesis::math::max(throughput) < thesis::device::consts::MIN_THROUGHPUT) {
            break;
        }
    }

    radiance /= static_cast<float>(params.image_.num_samples_per_pixel_);
    params.image_[pixel.y][pixel.x] = radiance;
}
