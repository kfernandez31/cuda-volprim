#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/utils/set.h"

#include <vector_types.h>

#include <cstddef>

namespace thesis {
namespace device {

template <size_t MaxPrims>
struct THESIS_ALIGNMENT ScatteringEvent {
    float t_hit_;
    float3 position_;
    float3 direction_;
    utils::Set<uint, MaxPrims> active_prims_;
};

}  // namespace device
}  // namespace thesis
