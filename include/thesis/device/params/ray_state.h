#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/params/prims_set.h"

#include <vector_types.h>

#include <cstdint>

namespace thesis {
namespace device {
namespace params {

// Per-(pixel, sample) path state for the wavefront path tracer (WAVEFRONT_PLAN.md Phase 1).
//
// In the megakernel these live as raygen registers/locals across the in-kernel bounce loop.
// The wavefront moves the bounce loop to the host (one optixLaunch per bounce), so this state
// must persist in global memory and be re-read/written every bounce. This struct IS the global
// per-ray record streamed each bounce — Phase 1 exists to measure exactly that traffic on the
// memory-latency axis we are already bound on (Risk R1).
//
// POD, identical layout on host and device. The host only allocates/zeroes the buffer; all
// field access is on the device. The RNG is stored as the raw PCG32 fields (two u64) rather than
// the device-only random::PCG32 type so this header stays host-includable.
//
// active_prims_ is the heavy field (~264 B CompactSet): it carries the scatter-point active set
// from one bounce to the next, which sample_scattering_event(first_bounce=false) requires (in the
// megakernel this is carried implicitly in the persistent `event` local). This is the dominant
// term in sizeof(RayState) and the primary thing Phase 1 weighs.
// Sentinel bounce value marking a finished (escaped / RR-killed / clamped) path. A ray with
// bounce_ == WF_RAY_DEAD early-outs of every subsequent bounce launch. Distinct from any real
// bounce index (max path depth ≪ 2^32).
inline constexpr uint32_t WF_RAY_DEAD = 0xFFFFFFFFu;

struct THESIS_ALIGNMENT RayState {
    // Per-ray bounce counter, FIRST so the dead-ray early-out reads the leading word. 0 = needs
    // init (first launch of the batch), k = resume at bounce k, WF_RAY_DEAD = finished. Storing
    // the bounce here (instead of a per-launch launch-param) lets the host upload launch params
    // ONCE per batch and just re-issue the same launch per bounce — no per-bounce host→device
    // param traffic (which, from a pageable copy, stalled ~60 ms/bounce; see FINDINGS/git log).
    uint32_t bounce_;

    float3 origin_;      // current ray origin
    float3 direction_;   // current ray direction (unit)
    float3 throughput_;  // path throughput
    float3 radiance_;    // accumulated radiance for this sample

    uint64_t rng_state_;  // PCG32 state_ (see device/core/random.cuh)
    uint64_t rng_inc_;    // PCG32 inc_

    // AOV running captures for the denoiser guide layers (written at bounce 0 only). Always
    // present so the struct layout is config-independent; finalize ignores them when AOVs are off.
    float3 aov_albedo_;
    float3 aov_normal_;

    // Scatter-point active set, carried bounce→bounce (origin-inside set for the next bounce).
    PrimsSet active_prims_;
};

}  // namespace params
}  // namespace device
}  // namespace thesis
