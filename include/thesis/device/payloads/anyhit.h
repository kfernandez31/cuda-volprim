#pragma once

#include "thesis/device/payloads/base.h"

#include <cuda_runtime.h>

namespace thesis {
namespace device {
namespace payloads {

struct THESIS_ALIGNMENT AnyHit : public Base<AnyHit, Tag::AnyHit> {
    // Tag (1) + buffer pointer (2 slots) + mode (1 slot) = 4 total slots (0-3)
    static constexpr size_t Count = 1 + 3;

    // Anyhit traversal mode (selects what the pointer points at and what the
    // anyhit program does during the single GAS descent).
    //   COLLECT       — ptr = HitBuffer*; append each entry hit (primary/scatter ray)
    //   TRANSMITTANCE — ptr = float*;     accumulate optical depth τ inline (shadow ray)
    static constexpr uint32_t MODE_COLLECT = 0;
    static constexpr uint32_t MODE_TRANSMITTANCE = 1;

    // Buffer/accumulator pointer stored in slots 1-2
    uint32_t buffer_ptr_low;   // Slot 1
    uint32_t buffer_ptr_high;  // Slot 2
    uint32_t mode;             // Slot 3

#ifdef __CUDA_ARCH__
    __device__ __forceinline__ void pack_impl(uint* out) const {
        out[0] = buffer_ptr_low;
        out[1] = buffer_ptr_high;
        out[2] = mode;
    }

    __device__ __forceinline__ void unpack_impl(const uint* in) {
        buffer_ptr_low = in[0];
        buffer_ptr_high = in[1];
        mode = in[2];
    }
#endif  // DEVICE
};

}  // namespace payloads
}  // namespace device
}  // namespace thesis
