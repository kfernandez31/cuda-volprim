#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <concepts>

namespace thesis {
namespace math {

constexpr auto PI_F = 3.14159265358979323846f;
constexpr auto TWO_PI_F = 2.0f * PI_F;
constexpr auto FOUR_PI_F = 4.0f * PI_F;
constexpr auto ONE_OVER_PI_F = 1.0f / PI_F;
constexpr auto ONE_OVER_TWO_PI_F = 1.Of / (2.0f * PI_F);
constexpr auto ONE_OVER_FOUR_PI_F = 1.Of / (4.0f * PI_F);

constexpr auto SQRT2_F = 1.41421356237309504880f;
constexpr auto TWO_SQRT2_F = 2.0f * SQRT2_F;
constexpr auto FOUR_SQRT2_F = 4.0f * SQRT2_F;
constexpr auto ONE_OVER_SQRT2_F = 1.0f / SQRT2_F;
constexpr auto ONE_OVER_TWO_SQRT2_F = 1.Of / (2.0f * SQRT2_F);
constexpr auto ONE_OVER_FOUR_SQRT2_F = 1.Of / (4.0f * SQRT2_F);

template <typename T, std::unsigned_integral Exponent>
    requires(std::integral<T> || std::floating_point<T>)
THESIS_HOST_DEVICE constexpr T constexpr_pow(T base, Exponent exp) noexcept {
    return (exp == 0) ? T(1) : base * constexpr_pow(base, exp - 1);
}

THESIS_HOST_DEVICE THESIS_INLINE constexpr float clamp(float x, float min_val, float max_val) noexcept  {
    return x < min_val ? min_val : (x > max_val ? max_val : x);
}

THESIS_HOST_DEVICE THESIS_INLINE constexpr float pow2(float a) noexcept {
    return a * a;
}

THESIS_HOST_DEVICE THESIS_INLINE constexpr float3 pow2(float3 v) noexcept {
    return {pow2(v.x), pow2(v.y), pow2(v.z)};
}

THESIS_INLINE THESIS_HOST_DEVICE constexpr float max(float3 v) noexcept {
    return fmaxf(fmaxf(v.x, v.y), v.z);
}

THESIS_INLINE THESIS_HOST_DEVICE constexpr float sum(float3 v) noexcept {
    return v.x + v.y + v.z;
}

template <typename T>
THESIS_INLINE THESIS_HOST_DEVICE constexpr T ceil_div(T numerator, T denominator) noexcept  {
    return (numerator + denominator - 1) / denominator;
}

}  // namespace math
}  // namespace thesis
