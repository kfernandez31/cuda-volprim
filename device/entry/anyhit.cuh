#pragma once

#include "core/constants.cuh"
#include "core/hit_record.cuh"
#include "core/launch_params.cuh"
#include "core/payload_utils.cuh"

#include "thesis/common/geometry/intersection.h"
#include "thesis/common/utils/math.h"
#include "thesis/common/utils/types.h"
#include "thesis/device/geometry/ray.h"
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
    namespace math = ::thesis::common::math;

    // Unpack AnyHit payload
    auto payload = payloads::AnyHit::unpackFromOptix();

    // TRANSMITTANCE mode (shadow rays): fuse the per-primitive optical-depth
    // integration into traversal. Each entry hit is integrated over its
    // [entry, exit] span and added to the τ accumulator in local memory; the
    // ray keeps traversing (optixIgnoreIntersection). This eliminates the
    // 128-deep HitBuffer for shadow rays — optical depth is ADDITIVE across
    // primitives, so order-independent inline accumulation is exact.
    if (payload.mode == payloads::AnyHit::MODE_TRANSMITTANCE) {
        const geometry::Ray ray = geometry::Ray::getCurrentRay();
        const float t_entry = optixGetRayTmax();
        const auto& prim = launch_params.primitives_[optixGetInstanceId()];
        const auto w = prim.transform_dir_local(ray.direction_);
        const float t_exit = ::thesis::common::geometry::compute_exit_from_entry(
            ray, t_entry, prim, math::length2(w));
        if (t_exit > t_entry && t_exit < consts::INF_F) {
            float* tau = unpack_ptr<float>(payload.buffer_ptr_low, payload.buffer_ptr_high);
            *tau += prim.optical_depth(ray, t_entry, t_exit);
        }
        optixIgnoreIntersection();
        return;
    }

    // COLLECT mode (primary/scatter rays): append entry hits to the buffer.
    auto* hit_buffer = unpack_ptr<HitBuffer>(payload.buffer_ptr_low, payload.buffer_ptr_high);

    // Collect entry hits (exits computed on-demand in argmin approach).
    // On overflow: drop the excess hit but keep traversing so the miss shader
    // can still deliver the env-map background. Terminating the ray here would
    // force a black background on top of the τ-truncation bias.
    if (hit_buffer->size() < consts::HIT_BUFFER_CAPACITY) {
        const float t = optixGetRayTmax();
        const prim_idx_t prim_idx = static_cast<prim_idx_t>(optixGetInstanceId());
        hit_buffer->emplace_back(t, prim_idx, false);
    } else {
        // Buffer full: this entry is dropped → primary ray under-samples this dense
        // region. Record the overflow so the host can warn (was previously silent).
        report_overflow();
    }
    optixIgnoreIntersection();
}
