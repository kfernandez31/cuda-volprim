#include <optix.h>

#include "common.cuh"
#include "util.cuh"

#include "thesis/optix/launch_params.h"

/*
extern "C" __global__ void __raygen__rg() {
    const auto idx = optixGetLaunchIndex();
    const auto pixel = make_float2(idx.x, idx.y);
    auto acc_color = make_float3(0.0f);

    for (size_t s = 0; s < params.num_samples_per_pixel_; ++s) {
        const auto jitter = sample_random_2d(idx, s);
        const auto ray = compute_jittered_ray(idx, jitter);
        acc_color += params.env_map_.sample(ray.direction);
    }

    acc_color /= static_cast<float>(params.num_samples_per_pixel_);
    params.image_(idx.x, idx.y) = acc_color;
}
*/

extern "C" __global__ void __raygen__rg() {
    const auto idx = optixGetLaunchIndex();
    const auto pixel = make_float2(idx.x, idx.y);
    auto acc_color = make_float3(0.0f);

    for (size_t s = 0; s < params.num_samples_per_pixel_; ++s) {
        const auto jitter = sample_random_2d(idx, s);
        const auto ray = compute_jittered_ray(idx, jitter);

        uint3 p;
        optixTrace(
            params.gas_handle_,
            ray.origin_,
            ray.direction_,
            0.0f,                                // Min intersection distance
            INF_F,                               // Max intersection distance
            0.0f,                                // Disable motion blur
            OptixVisibilityMask(VISIBILITY_ALL),
            OPTIX_RAY_FLAG_NONE,
            0,                                   // 0 - radiance, 1 - shadow, 2 - reflection
            thesis::optix::RAY_TYPE_COUNT,
            0,                                   // Use first miss program
            p.x, p.y, p.z
        );
    
        const auto result = make_float3(
            __uint_as_float(p.x),
            __uint_as_float(p.y),
            __uint_as_float(p.z)
        );

        acc_color += result;
    }

    acc_color /= static_cast<float>(params.num_samples_per_pixel_);
    params.image_(idx.x, idx.y) = acc_color;
}