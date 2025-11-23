#pragma once

#include "core/constants.cuh"
#include "core/payload_utils.cuh"
#include "core/hit_record.cuh"
#include "thesis/device/utils/vector.h"
#include "thesis/device/payloads/anyhit.h"
#include "thesis/common/utils/types.h"
#include "core/debug.cuh"

#include <optix.h>
#include <cstdint>

namespace thesis {
namespace device {
using HitBuffer = utils::StaticVector<HitRecord, consts::MAX_CAPACITY>;
}
}

extern "C" __global__ void __anyhit__ah() {
    using namespace thesis::device;

    // Unpack AnyHit payload
    auto payload = payloads::AnyHit::unpackFromOptix();

    auto* hit_buffer = unpack_ptr<HitBuffer>(payload.buffer_ptr_low, payload.buffer_ptr_high);

    if (!hit_buffer->full()) {
        const float t = optixGetRayTmax();
        const uint prim_idx = optixGetInstanceId();
        const bool is_exit = (optixGetHitKind() == OPTIX_HIT_KIND_TRIANGLE_BACK_FACE);

        if (is_debug_thread()) {
            printf("anyhit: t=%.6f, prim %u, exit=%d\n", t, prim_idx, is_exit);
        }

        hit_buffer->emplace_back(t, prim_idx, is_exit);
        optixIgnoreIntersection();
    } else {
        // Buffer full - terminate ray to prevent undefined behavior
        if (is_debug_thread()) {
            printf("anyhit: HIT BUFFER FULL (capacity=%u), terminating ray\n",
                   static_cast<uint>(consts::MAX_CAPACITY));
        }
        optixTerminateRay();
    }
}
