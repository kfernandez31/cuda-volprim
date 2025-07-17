#pragma once

#include "thesis/device/payloads/base.h"

#include <cuda_runtime.h>

namespace thesis {
namespace device {
namespace payloads {

struct THESIS_ALIGNMENT AnyHit : public Base<AnyHit, Tag::AnyHit> {
    static constexpr size_t Count = 1 + 0;

    // empty for now

#ifdef DEVICE
    __device__ void pack_impl(uint* out) const {}
    __device__ void unpack_impl(const uint* in) {}
#endif  // DEVICE
};

}  // namespace payloads
}  // namespace device
}  // namespace thesis
