#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <sutil/vec_math.h>

namespace thesis {
namespace common {
namespace math {

// Re-export dot() from sutil for consistency with other math functions
using ::dot;

constexpr float PI_F = 3.14159265358979323846f;
constexpr float TWO_PI_F = 2.0f * PI_F;
constexpr float FOUR_PI_F = 4.0f * PI_F;
constexpr float ONE_OVER_PI_F = 1.0f / PI_F;
constexpr float ONE_OVER_TWO_PI_F = 1.0f / (2.0f * PI_F);
constexpr float ONE_OVER_FOUR_PI_F = 1.0f / (4.0f * PI_F);
constexpr float ONE_OVER_TWO_PI_POW_3_2_F = 0.0634936359f;
constexpr float ROOT_TWO_PI_F = 2.5066282746f;

constexpr float ROOT_TWO_F = 1.41421356237309504880f;
constexpr float TWO_ROOT_TWO_F = 2.0f * ROOT_TWO_F;
constexpr float FOUR_ROOT_TWO_F = 4.0f * ROOT_TWO_F;
constexpr float ONE_OVER_ROOT_TWO_F = 1.0f / ROOT_TWO_F;
constexpr float ONE_OVER_TWO_ROOT_TWO_F = 1.0f / (2.0f * ROOT_TWO_F);
constexpr float ONE_OVER_FOUR_ROOT_TWO_F = 1.0f / (4.0f * ROOT_TWO_F);

template <typename T, typename Exponent>
THESIS_HOST_DEVICE constexpr T pow(T base, Exponent exp) noexcept {
    return (exp == 0) ? T(1) : base * pow(base, exp - 1);
}

THESIS_HOST_DEVICE THESIS_INLINE constexpr float pow2(float a) noexcept {
    return a * a;
}

THESIS_HOST_DEVICE THESIS_INLINE constexpr float3 pow2(float3 v) noexcept {
    return {pow2(v.x), pow2(v.y), pow2(v.z)};
}

template <typename T>
THESIS_HOST_DEVICE constexpr T min(T a) noexcept {
    return a;
}

template <typename T, typename... Ts>
THESIS_HOST_DEVICE constexpr T min(T a, Ts... args) noexcept {
    T m = min(args...);
    return a < m ? a : m;
}

THESIS_HOST_DEVICE THESIS_INLINE constexpr float min(float3 v) noexcept {
    return min(min(v.x, v.y), v.z);
}

template <typename T>
THESIS_HOST_DEVICE constexpr T max(T a) noexcept {
    return a;
}

template <typename T, typename... Ts>
THESIS_HOST_DEVICE constexpr T max(T a, Ts... args) noexcept {
    T m = max(args...);
    return a > m ? a : m;
}

THESIS_HOST_DEVICE THESIS_INLINE constexpr float max(float3 v) noexcept {
    return max(max(v.x, v.y), v.z);
}

THESIS_HOST_DEVICE THESIS_INLINE constexpr float clamp(float x, float lo, float hi) noexcept {
    return max(lo, min(x, hi));
}

THESIS_HOST_DEVICE THESIS_INLINE constexpr float sum(float3 v) noexcept {
    return v.x + v.y + v.z;
}

THESIS_HOST_DEVICE THESIS_INLINE constexpr float prod(float3 v) noexcept {
    return v.x * v.y * v.z;
}

THESIS_HOST_DEVICE THESIS_INLINE float length2(float3 v) noexcept {
    return dot(v, v);
}

// Compute next power of 2 >= n using bit manipulation
template <typename UInt>
THESIS_HOST_DEVICE THESIS_INLINE constexpr UInt next_power_of_2(UInt n) noexcept {
    static_assert(std::is_unsigned<UInt>::value, "next_power_of_2 requires unsigned integer type");

    if (n == 0)
        return 1;
    n--;

    // Unroll for all possible bit widths using if constexpr
    constexpr size_t bits = sizeof(UInt) * 8;
    if constexpr (bits >= 2)
        n |= n >> 1;
    if constexpr (bits >= 4)
        n |= n >> 2;
    if constexpr (bits >= 8)
        n |= n >> 4;
    if constexpr (bits >= 16)
        n |= n >> 8;
    if constexpr (bits >= 32)
        n |= n >> 16;
    if constexpr (bits >= 64)
        n |= n >> 32;

    return n + 1;
}

// Safe reciprocal: returns 0 for zero input (avoids div-by-zero)
THESIS_HOST_DEVICE THESIS_INLINE float safe_rcp(float x) noexcept {
    return (x != 0.0f) ? (1.0f / x) : 0.0f;
}

THESIS_HOST_DEVICE THESIS_INLINE float3 safe_rcp(float3 v) noexcept {
    return make_float3(safe_rcp(v.x), safe_rcp(v.y), safe_rcp(v.z));
}

// Sanitize scalar: clamp to non-negative and filter NaN/Inf
THESIS_HOST_DEVICE THESIS_INLINE float sanitize(float x) noexcept {
    x = fmaxf(x, 0.0f);             // Clamp negative
    return isfinite(x) ? x : 0.0f;  // Filter NaN/Inf
}

// Sanitize float3: component-wise sanitization
THESIS_HOST_DEVICE THESIS_INLINE float3 sanitize(float3 v) noexcept {
    return make_float3(sanitize(v.x), sanitize(v.y), sanitize(v.z));
}

template <typename T>
THESIS_HOST_DEVICE constexpr T ceil_div(T num, T den) noexcept {
    return (num + den - 1) / den;
}

}  // namespace math
}  // namespace common
}  // namespace thesis
