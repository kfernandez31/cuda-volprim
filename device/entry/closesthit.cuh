#pragma once

#ifndef OPTIX_HIT_KIND_SPHERE_FRONT_FACE
#   define OPTIX_HIT_KIND_SPHERE_FRONT_FACE 0xFEu
#   define OPTIX_HIT_KIND_SPHERE_BACK_FACE  0xFFu
#endif

#include "thesis/device/payloads/closesthit.h"

#include <optix.h>

extern "C" __global__ void __closesthit__ch() {
    using namespace thesis::device;

    payloads::ClosestHit p;
    p.t_hit    = optixGetRayTmax();
    p.prim_idx = optixGetInstanceId();
    // p.prim_idx = optixGetPrimitiveIndex(); // TODO(kacper): maybe use this?
    
    const unsigned hitKind = optixGetHitKind();
    p.is_exit = (hitKind == OPTIX_HIT_KIND_SPHERE_BACK_FACE);
    
    // More detailed debug output
    const auto idx = optixGetLaunchIndex();
    if (idx.x == 256 && idx.y == 256) {
        const float3 hitpoint = optixGetWorldRayOrigin() + optixGetRayTmax() * optixGetWorldRayDirection();
        printf("ClosestHit center pixel: t=%.3f, prim_idx=%u, hitKind=%u (0xFE=front, 0xFF=back)\n", 
               p.t_hit, p.prim_idx, hitKind);
        printf("  Hit point: (%.3f, %.3f, %.3f)\n", hitpoint.x, hitpoint.y, hitpoint.z);
    }

    p.packToOptix();
}