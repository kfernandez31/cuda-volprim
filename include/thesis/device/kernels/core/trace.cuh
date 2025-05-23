#pragma once

#include "thesis/device/geometry/ray.h"
#include "thesis/common/utils/types.h"
#include "thesis/device/utils/optional.h"

#include <optix.h>
#include <vector_types.h>

namespace thesis {
namespace device {

constexpr auto MAX_TRACE_PAYLOADS = 8u;
constexpr auto INF_F = 1e20f;
constexpr auto EPSILON = 1e-8f;
constexpr auto VISIBILITY_ALL = 0xFFu;

template <uint FLAGS, typename... Payloads>
__device__ __forceinline__ utils::Optional<float> trace(
    const Ray& ray,
    float t_min,
    Payloads&... payloads
) {
    static_assert(sizeof...(payloads) <= MAX_TRACE_PAYLOADS, "Too many payloads passed to trace_ch)");
    // clang-format off

    uint t_raw;      
    optixTrace(
        params.gas_handle_,
        ray.origin_,
        ray.direction_,
        t_min + EPSILON,  // Min intersection distance
        INF_F,  // Max intersection distance
        0.0f,   // Disable motion blur
        VISIBILITY_ALL,
        FLAGS,
        Ray::Type::RADIANCE,  // SBT offset
        Ray::Type::COUNT, // SBT stride
        0,  // miss SBT index: first miss program
        t_raw,
        payloads...);
    
    auto t = __uint_as_float(t_raw);
    if (t >= INF_F) {
        return utils::nullopt;
    }

    return t;
}

template <typename... Payloads>
__device__ __forceinline__ utils::Optional<float> trace_ch(
    const Ray& ray,
    float t_min,
    Payloads&... payloads
) {
    return trace<OPTIX_RAY_FLAG_DISABLE_ANYHIT>(ray, t_min, payloads...);
}

template <typename... Payloads>
__device__ __forceinline__ utils::Optional<float> trace_ah(
    const Ray& ray,
    float t_min,
    Payloads&... payloads
) {
    return trace<OPTIX_RAY_FLAG_ENFORCE_ANYHIT | OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT>(ray, t_min, payloads...);
}

} // namespace device
} // namespace thesis
