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
#ifdef __CUDACC__
    __device__ Miss(float _r, float _g, float _b) : r(_r), g(_g), b(_b) {}
    __device__ Miss(float3 color) : r(color.x), g(color.y), b(color.z) {}

    __forceinline__ __device__ float3 color() const noexcept { return make_float3(r, g, b); }

    __forceinline__ __device__ void pack_impl(uint* out) const noexcept {
        out[0] = __float_as_uint(r);
        out[1] = __float_as_uint(g);
        out[2] = __float_as_uint(b);
    }

    __forceinline__ __device__ void unpack_impl(const uint* in) noexcept {
        r = __uint_as_float(in[0]);
        g = __uint_as_float(in[1]);
        b = __uint_as_float(in[2]);
    }
#endif  // __CUDACC__
};

}  // namespace payloads
}  // namespace device
}  // namespace thesis
