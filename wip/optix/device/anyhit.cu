#include "thesis/optix/launch_params.h"

#include <optix.h>

extern "C" __global__ void __anyhit__ah() {
    const auto t = optixGetRayTmax();
    const auto prim_idx = optixGetPrimitiveIndex();
    const auto is_exit = optixIsTriangleBackFaceHit();

    optixSetPayload_0(__float_as_uint(t));
    optixSetPayload_1(prim_idx);
    optixSetPayload_2(is_exit);
}
