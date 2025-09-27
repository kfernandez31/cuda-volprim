#pragma once

#include "thesis/device/payloads/closesthit.h"
#include "thesis/device/geometry/ray.h"
#include "thesis/common/utils/math.h"

#include <optix.h>

extern "C" __global__ void __closesthit__ch() {
    using namespace thesis::device;
    payloads::ClosestHit p;
    p.t_hit    = optixGetRayTmax();
    p.prim_idx = optixGetInstanceId();
    
    // World-space ray
    const auto ray = geometry::Ray::getCurrentRay();

    // World-space hit point
    auto hit_point_world = ray.at(p.t_hit);

    // Inverse-transform the hit point from world space to object space
    auto hit_point_object = optixTransformPointFromWorldToObjectSpace(hit_point_world);

    // Compute normal in object space (sphere is centered at origin)
    auto normal_object = normalize(hit_point_object);

    // Transform normal to world space
    auto normal_world = optixTransformNormalFromObjectToWorldSpace(normal_object);

    // Check if ray is exiting
    p.is_exit = optixGetHitKind() == 1; // TODO(kacper): define the const somewhere shared
    if (p.is_exit) {
        printf("exited prim %u\n", p.prim_idx);
    } else {
        printf("entered prim %u\n", p.prim_idx);
    }

    p.packToOptix();
}
