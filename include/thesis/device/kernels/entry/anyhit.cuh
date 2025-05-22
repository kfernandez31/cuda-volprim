#pragma once

#include "thesis/device/kernels/launch_params.cuh"
#include "thesis/common/params/launch_params.h"
#include "thesis/common/utils/types.h"
#include "thesis/device/utils/data.h"

#include <optix.h>

namespace thesis {
namespace device {


// TODO(kacper): think of whether this should return a size_t
__forceinline__ __device__ uint getPrimitiveIndex() {
    const auto triangle_idx = optixGetPrimitiveIndex();
    const auto prim_idx = triangle_idx / params.num_triangles_per_primitive_;
    return prim_idx;
}

__forceinline__ __device__ getPayloadPointer() {
    uint32_t p0 = optixGetPayload_0();
    uint32_t p1 = optixGetPayload_1();
    return reinterpret_cast<optix::AnyhitPayload*>(data::unpackPointer(p0, p1));
}

} // namespace device
} // namespace thesis

extern "C" __global__ void __anyhit__ah() {
    auto* payload = getPayloadPointer();

    const float t = optixGetRayTmax();
    const uint prim_idx = thesis::device::getPrimitiveIndex();
    const bool is_exit = optixIsTriangleBackFaceHit();

    optixSetPayload_0(__float_as_uint(t));
    optixSetPayload_1(prim_idx);
    optixSetPayload_2(is_exit);

    if (!payload->events.full()) {
        payload->events.push_back({is_exit, t, prim_idx});
        optixIgnoreIntersection();  // Continue traversal
    }
    // else: implicitly accept this hit and stop traversal
}
