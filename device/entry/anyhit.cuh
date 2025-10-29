#pragma once

#include "core/constants.cuh"
#include "core/payload_utils.cuh"
#include "thesis/device/utils/set.h"
#include "thesis/common/utils/types.h"

#include <optix.h>
#include <cstdint>

extern "C" __global__ void __anyhit__ah() {
    using namespace thesis::device;

    // Unpack processed_this_t set pointer from payload slots 2 and 3
    uint32_t p0 = optixGetPayload_2();
    uint32_t p1 = optixGetPayload_3();
    auto* processed_this_t = unpack_ptr<utils::Set<uint, consts::MAX_PRIMS>>(p0, p1);

    // If no filter set provided, allow all hits
    if (!processed_this_t) {
        return;  // Accept hit, continue to closesthit
    }

    // Get primitive index
    uint prim_idx = optixGetInstanceId();

    // If already processed at current t, ignore this hit
    if (processed_this_t->contains(prim_idx)) {
        optixIgnoreIntersection();
    }
}
