#pragma once

#include "thesis/device/payloads/base.h"
#include <cuda_runtime.h>

namespace thesis {
namespace device {
namespace payloads {

struct THESIS_ALIGNMENT Miss : public Base<Miss, Tag::Miss> {
    static constexpr size_t Count = 1 + 3;

    float r, g, b;

#ifdef __CUDACC__
    THESIS_INLINE THESIS_HOST_DEVICE void pack_impl(uint* out) const noexcept {
        out[0] = __float_as_uint(r);
        out[1] = __float_as_uint(g);
        out[2] = __float_as_uint(b);
    }

    THESIS_INLINE THESIS_HOST_DEVICE void unpack_impl(const uint* in) noexcept {
        r = __uint_as_float(in[0]);
        g = __uint_as_float(in[1]);
        b = __uint_as_float(in[2]);
    }
#endif // __CUDACC__
};

}  // namespace payloads
}  // namespace device
}  // namespace thesis
