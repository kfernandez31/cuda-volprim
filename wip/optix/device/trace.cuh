#pragma once

#include "common.cuh"
#include <optix.h>
#include "thesis/device/ray.h"

namespace thesis {
namespace device {

constexpr auto MAX_TRACE_PAYLOADS = 8u;
constexpr auto INF_F = 1e20f

// TODO(kacper): constexpr here:
#define VISIBILITY_ALL 0xFF
        
template <typename... Payloads>
__device__ __forceinline__ void trace(
    const Ray& ray,
    float t_min,
    float t_max,
    Payloads&... payloads
) {
    static_assert(sizeof...(payloads) <= MAX_TRACE_PAYLOADS, "Too many payloads passed to trace()");
    optixTrace(
        params.gas_handle_,
        ray.origin_,
        ray.direction_,
        t_min,  // Min intersection distance
        t_max,  // Max intersection distance
        0.0f,   // Disable motion blur
        OptixVisibilityMask(VISIBILITY_ALL), // TODO(kacper): comment
        OPTIX_RAY_FLAG_DISABLE_ANYHIT_TERMINATION, // Allow many hits
        0,  // SBT offset: 0 - radiance, 1 - shadow, 2 - reflection
        RAY_TYPE_COUNT, // TODO(kacper): comment
        0,  // miss SBT index: first miss program
        payloads...);
}

} // namespace device
} // namespace thesis
