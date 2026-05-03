#pragma once

#include "core/constants.cuh"
#include "core/hit_record.cuh"
#include "core/payload_utils.cuh"

#include "thesis/common/utils/types.h"
#include "thesis/device/payloads/anyhit.h"
#include "thesis/device/payloads/miss.h"
#include "thesis/device/utils/vector.h"

#include <optix.h>

#include <cstdint>

namespace thesis {
namespace device {
using HitBuffer = utils::StaticVector<HitRecord, consts::HIT_BUFFER_CAPACITY>;
}
}  // namespace thesis

extern "C" __global__ void __anyhit__ah() {
    using namespace thesis::device;

    // Unpack AnyHit payload
    auto payload = payloads::AnyHit::unpackFromOptix();

    auto* hit_buffer = unpack_ptr<HitBuffer>(payload.buffer_ptr_low, payload.buffer_ptr_high);

    // Collect entry hits (exits computed on-demand in argmin approach).
    // On overflow: drop the excess hit but keep traversing so the miss shader
    // can still deliver the env-map background. Terminating the ray here would
    // force a black background on top of the τ-truncation bias.
    if (hit_buffer->size() < consts::HIT_BUFFER_CAPACITY) {
        const float t = optixGetRayTmax();
        const prim_idx_t prim_idx = static_cast<prim_idx_t>(optixGetInstanceId());
        hit_buffer->emplace_back(t, prim_idx, false);
    }
    optixIgnoreIntersection();
}
