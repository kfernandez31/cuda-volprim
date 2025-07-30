#pragma once

#include "core/launch_params.cuh"
#include "thesis/device/payloads/miss.h"

#include <optix.h>
#include <vector_types.h>

extern "C" __global__ void __miss__ms() {
    using namespace thesis::device;
    const auto ray_direction = optixGetWorldRayDirection();
    const auto color = launch_params.env_map_.sample(ray_direction);

    const auto idx = optixGetLaunchIndex();

    payloads::Miss p(color);
    p.packToOptix();
}
