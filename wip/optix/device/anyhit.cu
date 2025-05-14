#include "thesis/optix/launch_params.h"

#include <optix.h>

#include "common.cuh"

extern "C" __global__ void __anyhit__ah() {
    // Reject shadow rays, reflection rays, etc., if applicable // TODO(kacper): what?
    if (optixGetRayType() != 0) return;

    const auto t = optixGetRayTmax();
    const auto prim_idx = optixGetPrimitiveIndex();
    const auto is_exit = optixIsTriangleBackFaceHit();

    optixSetPayload_0(__float_as_uint(t));
    optixSetPayload_1(prim_idx);
    optixSetPayload_2(is_exit);

    // Accept intersection so closesthit can also be triggered if needed
    optixReportIntersection(t, 0);
}
