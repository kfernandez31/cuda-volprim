#pragma once

#include "thesis/device/payloads/base.h"

#include <cuda_runtime.h>
#include <vector_types.h>

namespace thesis {
namespace device {
namespace payloads {

struct THESIS_ALIGNMENT Miss : public Base<Miss, Tag::Miss> {
    static constexpr size_t Count = 1 + 3;

    float r, g, b;

    Miss() = default;
    Miss(const Miss&) = default;
    Miss& operator=(const Miss&) = default;
#ifdef DEVICE
    __device__ __forceinline__ Miss(float _r, float _g, float _b)
        : r(_r),
          g(_g),
          b(_b) {}
    __device__ __forceinline__ Miss(float3 color)
        : r(color.x),
          g(color.y),
          b(color.z) {}

    __device__ __forceinline__ float3 color() const { return make_float3(r, g, b); }

    __device__ __forceinline__ void pack_impl(uint* out) const {
        out[0] = __float_as_uint(r);
        out[1] = __float_as_uint(g);
        out[2] = __float_as_uint(b);
    }

    __device__ __forceinline__ void unpack_impl(const uint* in) {
        r = __uint_as_float(in[0]);
        g = __uint_as_float(in[1]);
        b = __uint_as_float(in[2]);
    }
#endif  // DEVICE
};

}  // namespace payloads
}  // namespace device
}  // namespace thesis
