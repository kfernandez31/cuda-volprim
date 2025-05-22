#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/common/utils/hit_event.h"

#include <cstddef>

static constexpr auto MAX_HIT_EVENTS = 64u;

namespace thesis {
namespace device {
namespace optix {

// TODO(kacper): remove?
struct THESIS_ALIGNMENT AnyhitPayload {
    utils::Vector<HitEvent, MAX_HIT_EVENTS> events;
};

}  // namespace optix
}  // namespace device
}  // namespace thesis
