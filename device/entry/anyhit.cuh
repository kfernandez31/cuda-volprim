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

    // Get primitive index and hit distance
    uint prim_idx = optixGetInstanceId();
    float t_hit = optixGetRayTmax();

    if (is_debug_thread()) {
        printf("anyhit: t=%.6f, prim %u, ptr=%p\n", t_hit, prim_idx, processed_this_t);
    }

    // If no filter set provided, allow all hits
    if (!processed_this_t) {
        if (is_debug_thread()) printf("  no filter set, allowing hit\n");
        return;  // Accept hit, continue to closesthit
    }

    // If already processed at current t, ignore this hit
    if (processed_this_t->contains(prim_idx)) {
        if (is_debug_thread()) printf("  prim %u already in set, IGNORING\n", prim_idx);
        optixIgnoreIntersection();
    } else {
        if (is_debug_thread()) printf("  prim %u not in set, allowing hit\n", prim_idx);
    }
}
