#pragma once

#include "thesis/device/payloads/base.h"
#include <cuda_runtime.h>

namespace thesis {
namespace device {
namespace payloads {

struct THESIS_ALIGNMENT ClosestHit : public Base<ClosestHit, Tag::ClosestHit> {
    static constexpr int Count = 1 + 3;

    float t_hit;
    uint prim_idx;
    bool is_exit;

#ifdef __CUDACC__
    __device__ void pack_impl(uint* out) const noexcept {
        out[0] = __float_as_uint(t_hit);
        out[1] = prim_idx;
        out[2] = static_cast<uint>(is_exit);
    }

    __device__ void unpack_impl(const uint* in) noexcept {
        t_hit = __uint_as_float(in[0]);
        prim_idx = in[1];
        is_exit = static_cast<bool>(in[2]);
    }
#endif // __CUDACC__
};

}  // namespace payloads
}  // namespace device
}  // namespace thesis
