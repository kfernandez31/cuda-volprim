#pragma once

#include "device/core/launch_params.cuh"
#include "thesis/common/params/launch_params.h"

#include <optix.h>
#include <vector_types.h>

extern "C" __global__ void __miss__ms() {
    const auto ray_direction = optixGetWorldRayDirection();
    const auto color = params.env_map_.sample(ray_direction);

    optixSetPayload_0(__float_as_uint(color.x)); // r
    optixSetPayload_1(__float_as_uint(color.y)); // g
    optixSetPayload_2(__float_as_uint(color.z)); // b
}