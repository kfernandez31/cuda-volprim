#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <cstddef>

namespace thesis {
namespace device {

// TODO(kacper): order of member vars
struct THESIS_ALIGNMENT HitEvent {
    bool is_entry_;
    float t_;
    size_t prim_idx_;

    struct Less {
        THESIS_INLINE THESIS_HOST_DEVICE constexpr bool operator()(const HitEvent& a, const HitEvent& b) const noexcept {
            return (a.t_ < b.t_) || (a.t_ == b.t_ && a.is_entry_ > b.is_entry_);
        }
    };
};

}  // namespace device
}  // namespace thesis
