#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/utils/set.h"

#include <cstddef>
#include <vector_types.h>

namespace thesis {
namespace device {

template <size_t Capacity>
struct THESIS_ALIGNMENT ScatteringEvent {
    float3 position_;
    float3 direction_;
    float t_;
    utils::Set<uint, Capacity> active_prims_;
};

}  // namespace device
}  // namespace thesis
