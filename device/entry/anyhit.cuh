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
using HitBuffer = utils::StaticVector<HitRecord, consts::HIT_BUFFER_CAPACITY>;
}
}

extern "C" __global__ void __anyhit__ah() {
    using namespace thesis::device;

    // Unpack AnyHit payload
    auto payload = payloads::AnyHit::unpackFromOptix();

    auto* hit_buffer = unpack_ptr<HitBuffer>(payload.buffer_ptr_low, payload.buffer_ptr_high);

    // Reserve space for exits: only collect up to MAX_PRIMITIVES entries
    // Each entry needs a corresponding exit (total = 2 × MAX_PRIMITIVES = HIT_BUFFER_CAPACITY)
    if (hit_buffer->size() < consts::MAX_PRIMITIVES) {
        const float t = optixGetRayTmax();
        const uint prim_idx = optixGetInstanceId();

        // With backface culling, all traced hits are entries (is_exit=false)
        if (is_debug_thread()) {
            printf("anyhit: entry t=%.6f, prim %u\n", t, prim_idx);
        }

        hit_buffer->emplace_back(t, prim_idx, false);
        optixIgnoreIntersection();
    } else {
        // Entry capacity reached - terminate ray to reserve space for exits
        if (is_debug_thread()) {
            printf("anyhit: ENTRY CAPACITY REACHED (%u entries, max=%u), terminating ray\n",
                   static_cast<uint>(hit_buffer->size()), static_cast<uint>(consts::MAX_PRIMITIVES));
        }
        optixTerminateRay();
    }
}
