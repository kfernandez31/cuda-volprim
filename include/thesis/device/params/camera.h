#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/geometry/ray.h"

#include <vector_types.h>

namespace thesis {
namespace device {

struct THESIS_ALIGNMENT Camera {
    float3 eye_;
    float3 pixel00_;
    float3 pixel_du_;
    float3 pixel_dv_;

#ifdef __CUDACC__
    __forceinline__ __device__ float3 ray_direction(uint2 pixel) const noexcept {
        return pixel00_ + (pixel.x * pixel_du_) + (pixel.y * pixel_dv_) - eye_;
    }

    __forceinline__ __device__ Ray jittered_ray(uint2 pixel, float2 jitter) const noexcept {
        const auto p = make_float2(pixel.x + jitter.x, pixel.y + jitter.y);
        const auto dir = ray_direction(p);
        return Ray::spawn(eye_, dir);
    }
#endif // __CUDACC__

};

}  // namespace device
}  // namespace thesis
