#pragma once

#include "thesis/device/kernels/launch_params.cuh"
#include "thesis/common/params/launch_params.h"
#include "thesis/device/utils/vector.h"
#include "thesis/device/utils/set.h"
#include "thesis/device/utils/math.h"
#include "thesis/common/utils/types.h"
#include "thesis/device/kernels/core/random.cuh"
#include "thesis/device/kernels/core/trace.cuh"

#include <optix.h>
#include <vector_types.h>
#include <sutil/vec_math.h>

namespace thesis {
namespace device {
namespace consts {

// TODO(kacper): select experimentally
constexpr auto MAX_HITS           = 64u;
constexpr auto MAX_BOUNCES         = 64u;
constexpr auto RUSSIAN_ROULETTE_DEPTH = 3u;
constexpr auto MIN_THROUGHPUT      = 1e-3f;
constexpr auto RR_MAX_SURVIVAL     = 0.99f;

} // namespace consts
} // namespace device
} // namespace thesis

namespace tdevice = thesis::device;

extern "C" __global__ void __raygen__rg() {
    const auto launch_idx = optixGetLaunchIndex();
    const auto pixel = make_uint2(launch_idx.x, launch_idx.y);
    const auto sample_idx = launch_idx.z;

    tdevice::optix::AnyhitPayload payload;

    // initialize rng
    curandState rng;
    curand_init(params.seed, pixel.y * params.image_width + pixel.x, sample_idx, &rng);

    const auto jitter = tdevice::random::sample_uniform_2d(&rng, 0.5f);
    auto ray = tdevice::Camera::jittered_ray(pixel, jitter);

    auto throughput = make_float3(1.0f);
    auto radiance = make_float3(0.0f);

    for (size_t bounce = 0; bounce < tdevice::consts::MAX_BOUNCES; ++bounce) {
        auto evt = tdevice::sample_scattering_event(ray, &rng);

        // no scattering - escaped mediums
        if (!evt) {
            auto tau = tdevice::compute_optical_depth_along_ray(ray);
            auto env = params.env_map_.sample(ray.direction_);
            radiance += throughput * expf(-tau) * env;
            break;
        }

        // Evaluate albedo and environment lighting
        auto albedo = tdevice::evaluate_albedo(evt->position_, evt->active_prims);
        auto env = params.env_map_.sample(evt->direction_);
        radiance += throughput * albedo * env * tdevice::phase_value();

        // Update energy by scattered amount (albedo)
        throughput *= albedo;

        // Russian Roulette
        if (bounce >= tdevice::consts::RUSSIAN_ROULETTE_DEPTH) {
            float p_survive = fminf(tdevice::consts::RR_MAX_SURVIVAL, tdevice::max(throughput));
            if (tdevice::random::sample_uniform(&rng) > p_survive) {
                break;
            }
            throughput /= p_survive;
        }

        // Prepare next ray
        ray = tdevice::Ray::spawn(evt->position_, evt->direction_);

        // TODO(kacper): I believe the first condition is unreachable
        if (!isfinite(tdevice::sum(throughput)) || tdevice::max(throughput) < tdevice::consts::MIN_THROUGHPUT) {
            break;
        }
    }

    radiance /= static_cast<float>(params.num_samples_per_pixel_);
    params.image_[idx.y][idx.x] = acc_radiance;
}
