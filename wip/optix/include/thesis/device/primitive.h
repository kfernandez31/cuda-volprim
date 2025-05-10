#pragma once

#include <vector_types.h>

namespace thesis {
namespace device {

struct Primitive {
    float3 color_;

    Primitive() = default;

    Primitive(float3 color) : color_(color) {}

#ifdef __CUDACC__
    __device__ float3 density_integral(float3 /* ray_origin */, float3 /* ray_direction */) const noexcept {
        return color_;
    }
#endif // __CUDACC__

};

} // namespace device
} // namespace thesis
