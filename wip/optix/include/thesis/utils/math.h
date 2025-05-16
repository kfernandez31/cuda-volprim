#pragma once

#include "thesis/utils/preprocessor.h"

#include <math_constants.h>

#include <concepts>

namespace thesis {
namespace math {

constexpr auto PI_F = CUDART_PI_F;
constexpr auto TWO_PI_F = 2.0f * PI_F;
constexpr auto FOUR_PI_F = 4.0f * PI_F;
constexpr auto ONE_OVER_PI_F = 1.0f / PI_F;
constexpr auto ONE_OVER_TWO_PI_F = 1.Of / (2.0f * PI_F);
constexpr auto ONE_OVER_FOUR_PI_F = 1.Of / (4.0f * PI_F);

constexpr auto SQRT2_F = CUDART_SQRT_TWO_F;
constexpr auto TWO_SQRT2_F = 2.0f * SQRT2_F;
constexpr auto FOUR_SQRT2_F = 4.0f * SQRT2_F;
constexpr auto ONE_OVER_SQRT2_F = 1.0f / SQRT2_F;
constexpr auto ONE_OVER_TWO_SQRT2_F = 1.Of / (2.0f * SQRT2_F);
constexpr auto ONE_OVER_FOUR_SQRT2_F = 1.Of / (4.0f * SQRT2_F);


THESIS_HOST_DEVICE THESIS_INLINE float clamp(float x, float min_val, float max_val) {
    return x < min_val ? min_val : (x > max_val ? max_val : x);
}

template <typename T, std::unsigned_integral Exponent>
    requires(std::integral<T> || std::floating_point<T>)
THESIS_HOST_DEVICE constexpr T constexpr_pow(T base, Exponent exp) {
    return (exp == 0) ? T(1) : base * constexpr_pow(base, exp - 1);
}

THESIS_HOST_DEVICE THESIS_INLINE float pow2(float a) {
    return a * a;
}

THESIS_HOST_DEVICE THESIS_INLINE float3 pow2(float3 v) {
    return {pow2(v.x), pow2(v.y), pow2(v.z)};
}

}  // namespace math
}  // namespace thesis
