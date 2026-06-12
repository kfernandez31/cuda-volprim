#pragma once

#include "thesis/common/params/launch_params.h"

extern "C" __constant__ thesis::common::params::LaunchParams launch_params;

namespace thesis {
namespace device {

// Indices into the 2-element overflow counter (host readout in renderer.cpp matches).
// Split per cap so the warning names the constant that was actually exceeded.
inline constexpr int OVERFLOW_HIT_BUFFER = 0;  // HIT_BUFFER_CAPACITY exceeded (anyhit drop)
inline constexpr int OVERFLOW_ACTIVE_SET = 1;  // MAX_ACTIVE_PRIMS exceeded (insert refused)

// Slot indices in LaunchParams::measure_buf_ (--measure-caps maxima).
inline constexpr int MEASURE_HIT_MAX = 0;     // max COLLECT-anyhit invocations per ray
inline constexpr int MEASURE_ACTIVE_MAX = 1;  // max point-overlap at a path vertex

// Record one cap-overflow event for the given cap. Atomically bumps the host-visible
// counter so a silently-biased dense-overlap render is surfaced as a warning. The atomic
// sits on the overflow branch only, so a correctly-sized run pays nothing. Null-guarded
// so it is a no-op if the counter is unset.
__device__ __forceinline__ void report_overflow(int which) {
    if (launch_params.overflow_counter_ != nullptr) {
        atomicAdd(&launch_params.overflow_counter_[which], 1ULL);
    }
}

}  // namespace device
}  // namespace thesis
