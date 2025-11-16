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
__device__ __forceinline__ auto trace_impl(
    const geometry::Ray& ray,
    float t_min,
    float t_max,
    utils::Set<uint, consts::MAX_PRIMS>* processed_this_t
) {
    uint ps[payloads::MAX_PAYLOADS]{};

    // Pack pointer into payload slots 2,3 for anyhit filtering
    if (processed_this_t) {
        pack_ptr(processed_this_t, ps[2], ps[3]);
    }

    optixTrace(
        launch_params.ias_handle_,
        ray.origin_,
        ray.direction_,
        t_min,                      // Min intersection distance (no epsilon - anyhit handles filtering)
        t_max,                      // Max intersection distance
        0.0f,                       // Disable motion blur
        consts::VISIBILITY_ALL,     // Visibility mask
        FLAGS,                      // Ray flags (enable anyhit for filtering)
        0,                          // SBT offset (single ray type)
        1,                          // SBT stride (single hit record per geometry)
        0,                          // miss SBT index: first miss program
        ps[0], ps[1], ps[2], ps[3]  // Payloads (change if MAX_PAYLOADS changes)
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

// Trace with closesthit and anyhit filtering - unbounded search
// Uses anyhit program to filter already-processed hits
__device__ __forceinline__ auto trace_ch(
    const geometry::Ray& ray,
    float t_min,
    utils::Set<uint, consts::MAX_PRIMS>* processed_this_t = nullptr
) {
    // Enable anyhit for filtering, keep closesthit, search to infinity
    return trace_impl<OPTIX_RAY_FLAG_NONE>(ray, t_min, consts::INF_F, processed_this_t);
}

// Trace with closesthit and anyhit filtering - bounded search within t-cluster
// Used to collect all hits at nearly the same t-value
__device__ __forceinline__ auto trace_ch_local(
    const geometry::Ray& ray,
    float t_min,
    float t_max,
    utils::Set<uint, consts::MAX_PRIMS>* processed_this_t = nullptr
) {
    // Enable anyhit for filtering, keep closesthit, bounded search
    return trace_impl<OPTIX_RAY_FLAG_NONE>(ray, t_min, t_max, processed_this_t);
}

} // namespace device
} // namespace thesis
