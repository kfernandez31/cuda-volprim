#pragma once

#include "core/primitive.cuh"
#include "thesis/device/payloads/closesthit.h"

#include <optix.h>

extern "C" __global__ void __closesthit__ch() {
    using namespace thesis::device;

    payloads::ClosestHit p;
    p.t_hit = optixGetRayTmax();
    p.prim_idx = getPrimitiveIndex();
    p.is_exit = optixIsTriangleBackFaceHit();

    p.packToOptix();
}
