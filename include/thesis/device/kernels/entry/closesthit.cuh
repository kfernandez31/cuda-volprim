#pragma once

#include "thesis/device/kernels/core/launch_params.cuh"
#include "thesis/common/params/launch_params.h"
#include "thesis/device/kernels/core/primitive.cuh"
#include "thesis/common/utils/types.h"

#include <optix.h>

extern "C" __global__ void __closesthit__ch() {
    const float t = optixGetRayTmax();
    const uint prim_idx = thesis::device::getPrimitiveIndex();
    const bool is_exit = optixIsTriangleBackFaceHit();

    optixSetPayload_0(__float_as_uint(t));
    optixSetPayload_1(prim_idx);
    optixSetPayload_2(is_exit);
}
