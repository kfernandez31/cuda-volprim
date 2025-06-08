#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/geometry/ray.h"

#include <vector_types.h>

namespace thesis {
namespace device {

class THESIS_ALIGNMENT Camera {
   private:
#ifdef __CUDACC__
    __forceinline__ __device__ float3 ray_direction(float2 jittered_pixel) const noexcept {
        return pixel00_ + (jittered_pixel.x * pixel_du_) + (jittered_pixel.y * pixel_dv_) - eye_;
    }
#endif  // __CUDACC__

   public:
    float3 eye_;
    float3 pixel00_;
    float3 pixel_du_;
    float3 pixel_dv_;
#ifdef __CUDACC__
    __forceinline__ __device__ geometry::Ray jittered_ray(uint2 pixel, float2 jitter) const noexcept {
        const auto p = make_float2(pixel.x + jitter.x, pixel.y + jitter.y);
        const auto dir = ray_direction(p);
        return geometry::Ray::spawn(eye_, dir);
    }
#endif  // __CUDACC__
};

}  // namespace device
}  // namespace thesis
