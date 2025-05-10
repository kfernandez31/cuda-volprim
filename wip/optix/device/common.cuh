#pragma once

#include <optix.h>
#include <cuda_runtime.h>

#include "thesis/optix/launch_params.h"

#define VISIBILITY_ALL 0xFF
#define INF_F 1e20f

extern "C" __constant__ thesis::optix::LaunchParams params;

// TODO(kacper): remove
__forceinline__ __device__ void setPayload(const float3& p) {
    optixSetPayload_0(__float_as_uint(p.x));
    optixSetPayload_1(__float_as_uint(p.y));
    optixSetPayload_2(__float_as_uint(p.z));
}
