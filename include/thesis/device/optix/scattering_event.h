#pragma once

#include "thesis/common/utils/preprocessor.h"

#include "thes"

#include <cstddef>

namespace thesis {
namespace device {

template <typename N>
struct THESIS_ALIGNMENT ScatteringEvent {
    float t_;
    float3 position_;
    float3 direction_;
    utils::Set<uint, N> active_prims_;
};

}  // namespace device
}  // namespace thesis


