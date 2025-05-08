#pragma once

#include "thesis/utils/preprocessor.h"
#include <math_constants.h>

namespace thesis {
namespace math {

constexpr auto PI_F = CUDART_PI_F;
constexpr auto INVPI_F = 1.0f / CUDART_PI_F;
constexpr auto HALF_INVPI_F = 0.5f * INVPI_F;

THESIS_HOST_DEVICE THESIS_INLINE float clamp(float x, float min_val, float max_val) {
    return x < min_val ? min_val : (x > max_val ? max_val : x);
}

} // namespace math
} // namespace thesis
