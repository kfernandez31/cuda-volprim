// TODO(kacper): return to this once closesthit works
#pragma once

extern "C" __global__ void __anyhit__ah() {}

/*

#include "thesis/device/kernels/core/launch_params.cuh"
#include "thesis/device/kernels/core/primitive.cuh"
#include "thesis/common/params/launch_params.h"
#include "thesis/common/utils/types.h"
#include "thesis/device/utils/data.h"
#include "thesis/device/optix/anyhit_payload.h"

#include <optix.h>
#include <cstddef>

namespace thesis {
namespace device {

template <size_t Capacity>
__forceinline__ __device__ optix::AnyhitPayload<Capacity>* getPayloadPointer() {
    uint32_t p0 = optixGetPayload_0();
    uint32_t p1 = optixGetPayload_1();
    return reinterpret_cast<optix::AnyhitPayload<Capacity>*>(data::unpackPointer(p0, p1));
}

} // namespace device
} // namespace thesis

extern "C" __global__ void __anyhit__ah() {
    // auto* payload = getPayloadPointer();
    thesis::device::optix::AnyhitPayload<1>* payload = nullptr; // TODO(kacper): fix

    const float t = optixGetRayTmax();
    const uint prim_idx = thesis::device::getPrimitiveIndex();
    const bool is_exit = optixIsTriangleBackFaceHit();

    optixSetPayload_0(__float_as_uint(t));
    optixSetPayload_1(prim_idx);
    optixSetPayload_2(is_exit);

    if (!payload->events.full()) {
        payload->events.emplace_back(prim_idx, t, is_exit);
        optixIgnoreIntersection();  // Continue traversal
    }
    // else: implicitly accept this hit and stop traversal
}

*/