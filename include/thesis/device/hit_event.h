#pragma once

#include "thesis/utils/preprocessor.h"

#include <cstddef>

namespace thesis {
namespace device {

struct THESIS_ALIGNMENT HitEvent {
    bool is_entry; // TODO(kacper): underscores
    float t;
    size_t prim_idx;

    struct Less {
        THESIS_INLINE THESIS_HOST_DEVICE constexpr bool operator()(const HitEvent& a, const HitEvent& b) const noexcept {
            return (a.t < b.t) || (a.t == b.t && a.is_entry > b.is_entry);
        }
    };
};

}  // namespace device
}  // namespace thesis
