#pragma once

#include "thesis/optix/launch_params.h"

#include <optix.h>
#include <vector_types.h>

extern "C" __constant__ thesis::optix::LaunchParams params;

enum RayType {
    RADIANCE = 0,
    SHADOW = 1,
    REFLECTION = 2,
    COUNT = 3,
};

// TODO(kacper): remove
__forceinline__ __device__ void setPayload(float3 p) {
    optixSetPayload_0(__float_as_uint(p.x));
    optixSetPayload_1(__float_as_uint(p.y));
    optixSetPayload_2(__float_as_uint(p.z));
}
