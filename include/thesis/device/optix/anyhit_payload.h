// TODO(kacper): return to this once closesthit works
#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/common/utils/hit_event.h"

#include <cstddef>

namespace thesis {
namespace device {
namespace optix {

template <size_t Capacity>
struct THESIS_ALIGNMENT AnyhitPayload {
    utils::Vector<HitEvent, Capacity> events;
};

}  // namespace optix
}  // namespace device
}  // namespace thesis
