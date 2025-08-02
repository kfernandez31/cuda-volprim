#pragma once

#include "thesis/device/geometry/ray.h"
#include "thesis/common/utils/types.h"
#include "thesis/device/utils/result.h"
#include "thesis/device/payloads/base.h"
#include "thesis/device/payloads/closesthit.h"

#include <optix.h>
#include <vector_types.h>

// TODO(kacper): think of optixReorder

namespace thesis {
namespace device {
namespace consts {
constexpr float INF_F = 1e20f;
constexpr uint VISIBILITY_ALL = 0xFFu;
constexpr float INTERSECTION_EPS = 1e-3f;
} // namespace consts

template <uint FLAGS>
__device__ __forceinline__ auto trace_impl(const geometry::Ray& ray, float t_min, float eps=consts::INTERSECTION_EPS) {
    uint ps[payloads::MAX_PAYLOADS] = {};
    
    optixTrace(
        launch_params.ias_handle_,
        ray.origin_,
        ray.direction_,
        t_min + eps,             // Min intersection distance (removed epsilon for debugging)
        consts::INF_F,                 // Max intersection distance
        0.0f,                  // Disable motion blur
        consts::VISIBILITY_ALL,
        FLAGS,
        0,                     // SBT offset (single ray type)
        1,                     // SBT stride (single hit record per geometry)
        0,                     // miss SBT index: first miss program
        ps[0], ps[1], ps[2], ps[3] // change if MAX_PAYLOADS changes
    );

    const auto tag = static_cast<payloads::Tag>(ps[0]);
    utils::Result<payloads::ClosestHit, payloads::Miss> result;

    if (tag == payloads::Tag::ClosestHit) {
        payloads::ClosestHit hit;
        hit.unpack(ps);
        result.emplace_ok(hit);
    } else {
        payloads::Miss miss;
        miss.unpack(ps);
        result.emplace_err(miss);
    }

    return result;
}

__device__ __forceinline__ auto trace_ch(const geometry::Ray& ray, float t_min) {
    return trace_impl<OPTIX_RAY_FLAG_DISABLE_ANYHIT>(ray, t_min);
}

__device__ __forceinline__ auto trace_ah(const geometry::Ray& ray, float t_min) {
    return trace_impl<OPTIX_RAY_FLAG_ENFORCE_ANYHIT | OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT>(ray, t_min);
}

} // namespace device
} // namespace thesis
