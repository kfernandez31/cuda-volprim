#pragma once

#include "core/launch_params.cuh"
#include "thesis/device/payloads/miss.h"

#include <optix.h>
#include <vector_types.h>

extern "C" __global__ void __miss__ms() {
    using namespace thesis::device;
    const auto ray_direction = optixGetWorldRayDirection();
    const auto ray_origin = optixGetWorldRayOrigin();
    const auto color = launch_params.env_map_.sample(ray_direction);

    const auto idx = optixGetLaunchIndex();
    
    // Debug output for center pixel
    if (launch_params.debug_ && idx.x == launch_params.image_.width_ / 2 && idx.y == launch_params.image_.height_ / 2) {
        printf("MISS: Ray origin=(%.3f,%.3f,%.3f) dir=(%.3f,%.3f,%.3f)\n", 
               ray_origin.x, ray_origin.y, ray_origin.z,
               ray_direction.x, ray_direction.y, ray_direction.z);
    }

    payloads::Miss p(color);
    p.packToOptix();
}
