#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/utils/set.h"

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
    utils::Set<uint, MaxPrims> active_prims_;
};

}  // namespace optix
}  // namespace device
}  // namespace thesis
