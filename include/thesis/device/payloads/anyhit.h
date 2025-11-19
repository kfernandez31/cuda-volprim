#pragma once

#include "thesis/device/payloads/base.h"

#include <cuda_runtime.h>

namespace thesis {
namespace device {
namespace payloads {

struct THESIS_ALIGNMENT AnyHit : public Base<AnyHit, Tag::AnyHit> {
    // Tag (1) + buffer pointer (2 slots) = 3 total slots (0-2)
    static constexpr size_t Count = 1 + 2;

    // Buffer pointer stored in slots 1-2
    uint32_t buffer_ptr_low;   // Slot 1
    uint32_t buffer_ptr_high;  // Slot 2

#ifdef DEVICE
    __device__ void pack_impl(uint* out) const {
        out[0] = buffer_ptr_low;
        out[1] = buffer_ptr_high;
    }

    __device__ void unpack_impl(const uint* in) {
        buffer_ptr_low = in[0];
        buffer_ptr_high = in[1];
    }
#endif  // DEVICE
};

}  // namespace payloads
}  // namespace device
}  // namespace thesis
