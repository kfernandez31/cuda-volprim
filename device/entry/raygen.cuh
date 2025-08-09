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

    // Debug output for center pixel
    if (launch_idx.x == 256 && launch_idx.y == 256) {
        printf("Raygen center pixel: origin=(%.3f,%.3f,%.3f) dir=(%.3f,%.3f,%.3f)\n",
               ray.origin_.x, ray.origin_.y, ray.origin_.z,
               ray.direction_.x, ray.direction_.y, ray.direction_.z);
    }
    
    const auto hit = trace_ch(ray, 0.0f);
    if (hit) {
        auto idx = hit.unwrap().prim_idx;
        radiance = launch_params.primitives_[idx].albedo_;
        
        // Debug output for any hit
        if (launch_idx.x == 256 && launch_idx.y == 256) {
            printf("Raygen: HIT sphere at t=%.3f, prim_idx=%u\n", 
                   hit.unwrap().t_hit, idx);
        }
    } else {
        radiance = hit.unwrap_err().color();
        
        // Debug output for miss
        if (launch_idx.x == 256 && launch_idx.y == 256) {
            printf("Raygen: MISS - using environment color\n");
        }
    }

    launch_params.image_[global_sample_idx] = radiance;
}
