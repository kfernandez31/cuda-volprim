#pragma once

#include "constants.cuh"
#include "hit_record.cuh"
#include "payload_utils.cuh"

#include "thesis/device/geometry/ray.h"
#include "thesis/device/payloads/anyhit.h"
#include "thesis/device/payloads/miss.h"
#include "thesis/device/utils/vector.h"

#include <optix.h>
#include <vector_types.h>

namespace thesis {
namespace device {

// Trace with anyhit collection mode - collects ALL hits into buffer
// Anyhit always calls optixIgnoreIntersection, causing Miss shader to execute and return env color
// The actual hit data is collected in the buffer via anyhit program
template <size_t N>
__device__ __forceinline__ payloads::Miss trace_ch_collect(
    const geometry::Ray& ray, float t_min, float t_max,
    utils::StaticVector<HitRecord, N>& hit_buffer) {
    // Pack buffer pointer into AnyHit payload
    payloads::AnyHit payload;
    pack_ptr(&hit_buffer, payload.buffer_ptr_low, payload.buffer_ptr_high);

    uint ps[payloads::MAX_PAYLOADS]{};
    payload.pack(ps);

    optixTrace(launch_params.ias_handle_, ray.origin_, ray.direction_, t_min, t_max,
               0.0f,                    // Disable motion blur
               consts::VISIBILITY_ALL,  // Visibility mask
               OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT |
                   OPTIX_RAY_FLAG_CULL_BACK_FACING_TRIANGLES,  // Disable closesthit, cull backfaces
               0,                                              // SBT offset (single ray type)
               1,                   // SBT stride (single hit record per geometry)
               0,                   // miss SBT index: first miss program
               ps[0], ps[1], ps[2], ps[3]  // Payloads (miss shader will overwrite with Miss payload)
    );

    // Unpack and return Miss payload from ps[] (modified by miss shader)
    payloads::Miss miss;
    miss.unpack(ps);
    return miss;
}

}  // namespace device
}  // namespace thesis
