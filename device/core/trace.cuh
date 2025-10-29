#pragma once

#include "constants.cuh"
#include "payload_utils.cuh"
#include "thesis/device/geometry/ray.h"
#include "thesis/device/utils/result.h"
#include "thesis/device/utils/set.h"
#include "thesis/device/payloads/base.h"
#include "thesis/device/payloads/closesthit.h"

#include <optix.h>
#include <vector_types.h>

// TODO(kacper): think of optixReorder

namespace thesis {
namespace device {

template <uint FLAGS>
__device__ __forceinline__ auto trace_impl(const geometry::Ray& ray, float t_min, float eps=consts::INTERSECTION_EPS) {
    uint ps[payloads::MAX_PAYLOADS]{};
    
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

// Filtered trace variant that uses anyhit to filter already-processed hits
template <uint FLAGS>
__device__ __forceinline__ auto trace_impl_filtered(
    const geometry::Ray& ray,
    float t_min,
    utils::Set<uint, consts::MAX_PRIMS>* processed_this_t,
    float eps = consts::INTERSECTION_EPS
) {
    uint ps[payloads::MAX_PAYLOADS]{};

    // Pack pointer into payload slots 2,3
    if (processed_this_t) {
        pack_ptr(processed_this_t, ps[2], ps[3]);
    }

    optixTrace(
        launch_params.ias_handle_,
        ray.origin_,
        ray.direction_,
        t_min + eps,
        consts::INF_F,
        0.0f,
        consts::VISIBILITY_ALL,
        FLAGS,
        0,
        1,
        0,
        ps[0], ps[1], ps[2], ps[3]
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

__device__ __forceinline__ auto trace_ch_filtered(
    const geometry::Ray& ray,
    float t_min,
    utils::Set<uint, consts::MAX_PRIMS>* processed_this_t = nullptr
) {
    // Enable anyhit for filtering, but keep closesthit
    return trace_impl_filtered<OPTIX_RAY_FLAG_NONE>(ray, t_min, processed_this_t);
}

// __device__ __forceinline__ auto trace_ah(const geometry::Ray& ray, float t_min) {
//     return trace_impl<OPTIX_RAY_FLAG_ENFORCE_ANYHIT | OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT>(ray, t_min);
// }

} // namespace device
} // namespace thesis
