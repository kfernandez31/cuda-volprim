#pragma once

#include "thesis/common/utils/math.h"
#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/geometry/ray.h"

#include <vector_types.h>

namespace thesis {
namespace device {
namespace params {

class THESIS_ALIGNMENT Camera {
#ifdef DEVICE
    __device__ __forceinline__ float3 ray_direction(float2 jittered_pixel) const {
        // Compute: pixel00_relative + x*pixel_du + y*pixel_dv
        // Using nested FMA for optimal performance (6 fused multiply-add instructions):
        //   Inner FMA: y*pixel_dv + pixel00_relative
        //   Outer FMA: x*pixel_du + (result from inner)
        return common::math::fmaf(
            jittered_pixel.x, pixel_du_,
            common::math::fmaf(jittered_pixel.y, pixel_dv_, pixel00_relative_));
    }
#endif  // DEVICE

   public:
    float3 eye_;
    float3 pixel00_relative_;  // Precomputed: pixel00 - eye (saves 3 subtractions per ray)
    float3 pixel_du_;
    float3 pixel_dv_;

#ifdef DEVICE
    __device__ geometry::Ray jittered_ray(uint2 pixel, float2 jitter) const {
        const auto p = make_float2(pixel.x + jitter.x, pixel.y + jitter.y);
        const auto dir = ray_direction(p);

        // TODO(optimization): Ray::spawn() calls normalize(dir), costing ~13 ops per ray
        // If pixel_du_ and pixel_dv_ were pre-normalized and scaled appropriately during
        // camera setup, we could use spawn_unchecked() and save the normalization.
        //
        // Requirements:
        // - Precompute normalized basis vectors on host
        // - Adjust ray_direction() to return pre-normalized direction
        // - Verify correctness (camera model must account for viewport scaling)
        //
        // Expected savings: ~13 ops per primary ray (normalize = dot + rsqrt + 3 muls)
        return geometry::Ray::spawn(eye_, dir);
    }

    __device__ float3 position() const { return eye_; }
#endif  // DEVICE
};

}  // namespace params
}  // namespace device
}  // namespace thesis
