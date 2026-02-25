#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/utils/bit_vector.h"

#include <vector_types.h>

#include <cstddef>

namespace thesis {
namespace device {
namespace optix {

template <size_t MaxPrims>
struct THESIS_ALIGNMENT ScatteringEvent {
    float3 position_;
    float3 direction_;
    float t_hit_;
    utils::BitVector<MaxPrims> active_prims_;
    float3 escape_optical_depth_;  // Optical depth for escape case (segment-by-segment integration)
};

}  // namespace optix
}  // namespace device
}  // namespace thesis
