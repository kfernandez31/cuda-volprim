#pragma once

#include "launch_params.cuh"
#include "thesis/optix/launch_params.h"

#include <optix.h>

namespace thesis {
namespace device {

__forceinline__ __device__ size_t getPrimitiveIndex() {
    const auto triangle_idx = optixGetPrimitiveIndex();
    const auto prim_idx = triangle_idx / params.num_triangles_per_primitive_;
    return prim_idx;
}

} // namespace device
} // namespace thesis

extern "C" __global__ void __anyhit__ah() {
    const auto t = optixGetRayTmax();
    const auto prim_idx = thesis::device::getPrimitiveIndex();
    const auto is_exit = optixIsTriangleBackFaceHit();

    optixSetPayload_0(__float_as_uint(t));
    optixSetPayload_1(prim_idx);
    optixSetPayload_2(is_exit);
}
