#pragma once

#include "thesis/device/payloads/closesthit.h"
#include "thesis/device/geometry/ray.h"
#include "thesis/common/utils/math.h"

#include <optix.h>

extern "C" __global__ void __closesthit__ch()
{
    using namespace thesis::device;
    payloads::ClosestHit p;
    p.t_hit    = optixGetRayTmax();
    p.prim_idx = optixGetInstanceId();

    const auto hk = optixGetHitKind();
    p.is_exit = (hk == OPTIX_HIT_KIND_TRIANGLE_BACK_FACE);

    { // remove me
        const uint3 launch_index = optixGetLaunchIndex();
        const uint3 launch_dim   = optixGetLaunchDimensions();

        const uint32_t mid_x = launch_dim.x / 2;
        const uint32_t mid_y = launch_dim.y / 2;

        if (launch_index.x == mid_x && launch_index.y == mid_y) {
            printf("%s prim %u (hitKind=%u)\n", p.is_exit ? "exited" : "entered", p.prim_idx, hk);
        }
    } // remove me

    p.packToOptix();
}


// TODO(kacper): remove if unused
/*
extern "C" __global__ void __closesthit__ch()
{
    using namespace thesis::device;
    payloads::ClosestHit p;
    p.t_hit    = optixGetRayTmax();
    p.prim_idx = optixGetInstanceId();

    // World-space ray
    const auto ray = geometry::Ray::getCurrentRay();
    auto hit_point_world = ray.at(p.t_hit);

    // Compute normal in object space and transform to world
    auto hit_point_object = optixTransformPointFromWorldToObjectSpace(hit_point_world);
    auto normal_object    = normalize(hit_point_object);
    auto normal_world     = normalize(optixTransformNormalFromObjectToWorldSpace(normal_object));

    // Decide entry/exit from dot(ray.dir, normal)
    const float d = dot(ray.direction_, normal_world);
    p.is_exit = (d > 0.0f);

    printf("%s prim %u (dot=%.3f)\n",
           p.is_exit ? "exited" : "entered",
           p.prim_idx, d);

    p.packToOptix();
}
*/