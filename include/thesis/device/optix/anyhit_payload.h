// TODO(kacper): return to this once closesthit works
#pragma once

#include "thesis/device/optix/hit_event.h"
#include "thesis/device/utils/vector.h"
#include "thesis/common/utils/preprocessor.h"

#include <cstddef>

namespace thesis {
namespace device {
namespace optix {

template <size_t Capacity>
struct THESIS_ALIGNMENT AnyhitPayload {
    utils::StaticVector<HitEvent, Capacity> events;
};

}  // namespace optix
}  // namespace device
}  // namespace thesis
