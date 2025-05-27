#pragma once

#include "thesis/device/payloads/base.h"

#include <cuda_runtime.h>

namespace thesis {
namespace device {
namespace payloads {

struct THESIS_ALIGNMENT AnyHit : public Base<AnyHit, Tag::AnyHit> {
    static constexpr int Count = 1;

    // empty for now

#ifdef __CUDACC__
    __forceinline__ __device__ void pack_impl(uint* out) const noexcept {}

    __forceinline__ __device__ void unpack_impl(const uint* in) noexcept {}
#endif  // __CUDACC__
};

}  // namespace payloads
}  // namespace device
}  // namespace thesis
