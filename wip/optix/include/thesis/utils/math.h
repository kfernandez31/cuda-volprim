#pragma once

#include "thesis/utils/preprocessor.h"

#include <math_constants.h>
#include <concepts>

namespace thesis {
namespace math {

constexpr auto PI_F = CUDART_PI_F;
constexpr auto INVPI_F = 1.0f / CUDART_PI_F;
constexpr auto HALF_INVPI_F = 0.5f * INVPI_F;

THESIS_HOST_DEVICE THESIS_INLINE float clamp(float x, float min_val, float max_val) {
    return x < min_val ? min_val : (x > max_val ? max_val : x);
}

template <typename T, std::unsigned_integral Exponent>
    requires (std::integral<T> || std::floating_point<T>)
THESIS_HOST_DEVICE constexpr T constexpr_pow(T base, Exponent exp) {
    return (exp == 0) ? T(1) : base * constexpr_pow(base, exp - 1);
}

}  // namespace math
}  // namespace thesis
