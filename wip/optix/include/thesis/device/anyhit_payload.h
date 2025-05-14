#pragma once

#include "thesis/utils/preprocessor.h"
#include "thesis/utils/hit_event.h"

#include <cstddef>

#define MAX_HIT_EVENTS 64

namespace thesis {
namespace device {

struct THESIS_ALIGNMENT Payload {
    HitEvent events[MAX_HIT_EVENTS];
    uint32_t count;
};

}  // namespace device
}  // namespace thesis
