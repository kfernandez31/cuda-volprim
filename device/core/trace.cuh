#pragma once

#include "constants.cuh"
#include "payload_utils.cuh"
#include "hit_record.cuh"
#include "thesis/device/geometry/ray.h"
#include "thesis/device/utils/vector.h"
#include "thesis/device/payloads/anyhit.h"

#include <optix.h>
#include <vector_types.h>

namespace thesis {
namespace device {

// Trace with anyhit collection mode - collects ALL hits into buffer
// Anyhit always calls optixIgnoreIntersection, so this always returns Miss
// The actual hit data is collected in the buffer via anyhit program
template <size_t N>
__device__ __forceinline__ void trace_ch_collect(
    const geometry::Ray& ray,
    float t_min,
    float t_max,
    utils::StaticVector<HitRecord, N>* hit_buffer
) {
    // Pack buffer pointer into AnyHit payload
    payloads::AnyHit payload;
    pack_ptr(hit_buffer, payload.buffer_ptr_low, payload.buffer_ptr_high);

    uint ps[payloads::MAX_PAYLOADS]{};
    payload.pack(ps);

    optixTrace(
        launch_params.ias_handle_,
        ray.origin_,
        ray.direction_,
        t_min,
        t_max,
        0.0f,                       // Disable motion blur
        consts::VISIBILITY_ALL,     // Visibility mask
        OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT,  // Disable closesthit (anyhit handles everything)
        0,                          // SBT offset (single ray type)
        1,                          // SBT stride (single hit record per geometry)
        0,                          // miss SBT index: first miss program
        ps[0], ps[1], ps[2]  // Tag (0) + buffer pointer (1-2)
    );

    // TODO(kacper): maybe the following can be optimized, since we don't read from the payload if we got some prims in the buffer
    // Note: Always returns Miss since anyhit ignores everything
    // Hit data is in the buffer
}

} // namespace device
} // namespace thesis
