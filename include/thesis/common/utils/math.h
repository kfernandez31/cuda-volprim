#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <sutil/vec_math.h>

namespace thesis {
namespace common {
namespace math {

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

template <typename T>
THESIS_HOST_DEVICE constexpr T ceil_div(T num, T den) noexcept {
    return (num + den - 1) / den;
}

}  // namespace math
}  // namespace common
}  // namespace thesis
