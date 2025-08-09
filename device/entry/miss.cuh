#pragma once

#include "core/launch_params.cuh"
#include "thesis/device/payloads/miss.h"

#include <optix.h>
#include <vector_types.h>

extern "C" __global__ void __miss__ms() {
    using namespace thesis::device;

    const auto idx = optixGetLaunchIndex();
    if (idx.x == 256 && idx.y == 256) {
        const float3 ray_origin = optixGetWorldRayOrigin();
        const float3 ray_dir = optixGetWorldRayDirection();
        printf("MISS at [256,256]: origin=(%.3f,%.3f,%.3f) dir=(%.3f,%.3f,%.3f)\n",
               ray_origin.x, ray_origin.y, ray_origin.z,
               ray_dir.x, ray_dir.y, ray_dir.z);
    }
    
    auto color = make_float3(0.0f, 0.0f, 1.0f);
    payloads::Miss p(color);
    p.packToOptix();
}
