#pragma once

#include "thesis/common/utils/types.h"

namespace thesis {
namespace device {
namespace consts {

// Ray tracing constants
constexpr float INF_F = 1e20f;
constexpr uint VISIBILITY_ALL = 0xFFu;
constexpr float INTERSECTION_EPS = 1e-3f;

// Primitive and scattering constants
constexpr size_t MAX_PRIMS = 10u;  // Maximum primitives ray can be inside simultaneously

} // namespace consts
} // namespace device
} // namespace thesis
