#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <cstddef>

namespace thesis {
namespace device {
namespace optix {

struct THESIS_ALIGNMENT HitEvent {
    float t_;
    bool is_entry_;
    size_t prim_idx_;

    struct Less {
        __device__ constexpr bool operator()(const HitEvent& a, const HitEvent& b) const noexcept {
            return (a.t_ < b.t_) || (a.t_ == b.t_ && a.is_entry_ > b.is_entry_);
        }
    };
};

}  // namespace optix
}  // namespace device
}  // namespace thesis
