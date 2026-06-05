#pragma once

#include "thesis/common/params/launch_params.h"

extern "C" __constant__ thesis::common::params::LaunchParams launch_params;

namespace thesis {
namespace device {

// Record one cap-overflow event (active-prims CompactSet full, or HitBuffer full).
// Atomically bumps the host-visible counter so a silently-biased dense-overlap render
// is surfaced as a warning. Null-guarded so it is a no-op if the counter is unset.
__device__ __forceinline__ void report_overflow() {
    if (launch_params.overflow_counter_ != nullptr) {
        atomicAdd(launch_params.overflow_counter_, 1ULL);
    }
}

}  // namespace device
}  // namespace thesis
