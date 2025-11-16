#pragma once

#include "core/constants.cuh"
#include "core/payload_utils.cuh"
#include "thesis/device/utils/set.h"
#include "thesis/common/utils/types.h"
#include "core/debug.cuh"

#include <optix.h>
#include <cstdint>

namespace thesis {
namespace device {

// Helper to unpack processed_this_t set pointer from OptiX payload
__device__ __forceinline__ const utils::Set<uint, consts::MAX_PRIMS>* get_processed_set() {
    uint32_t p0 = optixGetPayload_2();
    uint32_t p1 = optixGetPayload_3();
    return unpack_ptr<utils::Set<uint, consts::MAX_PRIMS>>(p0, p1);
}

} // namespace device
} // namespace thesis

extern "C" __global__ void __anyhit__ah() {
    using namespace thesis::device;

    // Unpack processed_this_t set pointer from payload
    const auto* processed_this_t = get_processed_set();

    // If no filter set provided, allow all hits
    if (!processed_this_t) {
        return;  // Accept hit, continue to closesthit
    }

    // Get primitive index
    uint prim_idx = optixGetInstanceId();

    if (is_debug_thread()) {
        float t_hit = optixGetRayTmax();
        printf("anyhit: t=%.6f, prim %u, in_set=%d\n",
               t_hit, prim_idx, processed_this_t->contains(prim_idx));
    }

    // Filter if we've already processed this primitive at current t-cluster
    // The bounded t_max in trace_ch_local ensures we only see hits within the cluster
    if (processed_this_t->contains(prim_idx)) {
        if (is_debug_thread()) printf("  prim %u already processed, IGNORING\n", prim_idx);
        optixIgnoreIntersection();
    } else {
        if (is_debug_thread()) printf("  prim %u will be processed, ACCEPTING\n", prim_idx);
    }
}
