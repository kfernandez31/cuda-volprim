#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <vector_types.h>

#include <cstddef>

namespace thesis {
namespace device {
namespace optix {

template <typename PrimsSetT>
struct THESIS_ALIGNMENT ScatteringEvent {
    float3 position_;
    float3 direction_;
    float t_hit_;
    PrimsSetT active_prims_;
};

}  // namespace optix
}  // namespace device
}  // namespace thesis
