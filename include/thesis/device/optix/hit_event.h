#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <cstddef>

namespace thesis {
namespace device {

struct THESIS_ALIGNMENT HitEvent {
    size_t prim_idx_;
    float t_;
    bool is_entry_;

    struct Less {
        THESIS_INLINE THESIS_HOST_DEVICE constexpr bool operator()(
            const HitEvent& a, const HitEvent& b) const noexcept {
            return (a.t_ < b.t_) || (a.t_ == b.t_ && a.is_entry_ > b.is_entry_);
        }
    };
};

}  // namespace device
}  // namespace thesis
