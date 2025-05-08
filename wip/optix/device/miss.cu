#include "thesis/optix/launch_params.h"

#include <optix.h>

#include "common.cuh"

extern "C" __global__ void __miss__ms() {
    const auto ray_direction = optixGetWorldRayDirection();
    const auto color = params.env_map_.sample(ray_direction);
    setPayload(color);
}