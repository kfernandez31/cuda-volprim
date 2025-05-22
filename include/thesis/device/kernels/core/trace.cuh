#pragma once

#include "thesis/device/geometry/ray.h"

#include <optix.h>
#include <vector_types.h>

namespace thesis {
namespace device {

constexpr auto MAX_TRACE_PAYLOADS = 8u;
constexpr auto INF_F = 1e20f;
constexpr auto VISIBILITY_ALL = 0xFFu;

template <typename... Payloads>
__device__ __forceinline__ void trace(
    const Ray& ray,
    float2 t,
    Payloads&... payloads
) {
    static_assert(sizeof...(payloads) <= MAX_TRACE_PAYLOADS, "Too many payloads passed to trace()");
    // clang-format off
    optixTrace(
        params.gas_handle_,
        ray.origin_,
        ray.direction_,
        t.x,  // Min intersection distance
        t.y,  // Max intersection distance
        0.0f,   // Disable motion blur
        VISIBILITY_ALL,
        OPTIX_RAY_FLAG_ENFORCE_ANYHIT | OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT,
        Ray::Type::RADIANCE,  // SBT offset
        Ray::Type::COUNT, // SBT stride
        0,  // miss SBT index: first miss program
        payloads...);
}

} // namespace device
} // namespace thesis
