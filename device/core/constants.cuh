#pragma once

#include "thesis/common/utils/types.h"

namespace thesis {
namespace device {
namespace consts {

// Ray tracing constants
constexpr float INF_F = 1e20;
constexpr uint VISIBILITY_ALL = 0xFF;

// Primitive and scattering constants
// Maximum capacity for both hit collection buffer and active primitives set
constexpr size_t MAX_CAPACITY = 256;

} // namespace consts
} // namespace device
} // namespace thesis
