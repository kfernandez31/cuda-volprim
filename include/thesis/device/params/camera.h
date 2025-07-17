#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/geometry/ray.h"

#include <vector_types.h>

namespace thesis {
namespace device {
namespace params {

class THESIS_ALIGNMENT Camera {
#ifdef DEVICE
    __device__ float3 ray_direction(float2 jittered_pixel) const {
        return pixel00_ + (jittered_pixel.x * pixel_du_) + (jittered_pixel.y * pixel_dv_) - eye_;
    }
#endif  // DEVICE

   public:
    float3 eye_;
    float3 pixel00_;
    float3 pixel_du_;
    float3 pixel_dv_;

#ifdef DEVICE
    __device__ geometry::Ray jittered_ray(uint2 pixel, float2 jitter) const {
        const auto p = make_float2(pixel.x + jitter.x, pixel.y + jitter.y);
        const auto dir = ray_direction(p);
        return geometry::Ray::spawn(eye_, dir);
    }
#endif  // DEVICE
};

}  // namespace params
}  // namespace device
}  // namespace thesis
