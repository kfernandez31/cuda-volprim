#pragma once

#include "thesis/device/payloads/closesthit.h"
#include "thesis/device/geometry/ray.h"
#include "thesis/common/utils/math.h"
#include "core/debug.cuh"

#include <optix.h>

extern "C" __global__ void __closesthit__ch()
{
    using namespace thesis::device;
    payloads::ClosestHit p;
    p.t_hit    = optixGetRayTmax();
    p.prim_idx = optixGetInstanceId();

    const auto hk = optixGetHitKind();
    p.is_exit = (hk == OPTIX_HIT_KIND_TRIANGLE_BACK_FACE);

    if (is_debug_thread()) {
        printf("%s prim %u (hitKind=%u)\n", p.is_exit ? "exited" : "entered", p.prim_idx, hk);
    }

    p.packToOptix();
}
