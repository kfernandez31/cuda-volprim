#pragma once

#include "thesis/common/utils/preprocessor.h"

namespace thesis {
namespace common {
namespace math {

constexpr auto PI_F = 3.14159265358979323846f;
constexpr auto TWO_PI_F = 2.0f * PI_F;
constexpr auto FOUR_PI_F = 4.0f * PI_F;
constexpr auto ONE_OVER_PI_F = 1.0f / PI_F;
constexpr auto ONE_OVER_TWO_PI_F = 1.0f / (2.0f * PI_F);
constexpr auto ONE_OVER_FOUR_PI_F = 1.0f / (4.0f * PI_F);

constexpr auto ROOT_TWO_F = 1.41421356237309504880f;
constexpr auto TWO_ROOT_TWO_F = 2.0f * ROOT_TWO_F;
constexpr auto FOUR_ROOT_TWO_F = 4.0f * ROOT_TWO_F;
constexpr auto ONE_OVER_ROOT_TWO_F = 1.0f / ROOT_TWO_F;
constexpr auto ONE_OVER_TWO_ROOT_TWO_F = 1.0f / (2.0f * ROOT_TWO_F);
constexpr auto ONE_OVER_FOUR_ROOT_TWO_F = 1.0f / (4.0f * ROOT_TWO_F);

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
THESIS_INLINE THESIS_HOST_DEVICE constexpr T min(T a) noexcept {
    return a;
}

template <typename T, typename... Ts>
THESIS_INLINE THESIS_HOST_DEVICE constexpr T min(T a, Ts... args) noexcept {
    T m = min(args...);
    return a < m ? a : m;
}

THESIS_INLINE THESIS_HOST_DEVICE constexpr float min(float3 v) noexcept {
    return min(min(v.x, v.y), v.z);
}

template <typename T>
THESIS_INLINE THESIS_HOST_DEVICE constexpr T max(T a) noexcept {
    return a;
}

template <typename T, typename... Ts>
THESIS_INLINE THESIS_HOST_DEVICE constexpr T max(T a, Ts... args) noexcept {
    T m = max(args...);
    return a > m ? a : m;
}

THESIS_INLINE THESIS_HOST_DEVICE constexpr float max(float3 v) noexcept {
    return max(max(v.x, v.y), v.z);
}

THESIS_HOST_DEVICE THESIS_INLINE constexpr float clamp(float x, float lo, float hi) noexcept {
    return max(lo, min(x, hi));
}

THESIS_INLINE THESIS_HOST_DEVICE constexpr float sum(float3 v) noexcept {
    return v.x + v.y + v.z;
}

THESIS_INLINE THESIS_HOST_DEVICE constexpr float prod(float3 v) noexcept {
    return v.x * v.y * v.z;
}

template <typename T>
THESIS_INLINE THESIS_HOST_DEVICE constexpr T ceil_div(T num, T den) noexcept {
    return (num + den - 1) / den;
}

}  // namespace math
}  // namespace common
}  // namespace thesis
