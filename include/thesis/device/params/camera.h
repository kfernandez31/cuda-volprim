#pragma once

#include "thesis/common/utils/math.h"
#include "thesis/common/utils/preprocessor.h"

#include <vector_types.h>

#ifdef DEVICE
#include "thesis/device/geometry/ray.h"
#endif

namespace thesis {
namespace device {
namespace params {

// Device-side POD struct for camera (no RAII, same size on host and device)
struct THESIS_ALIGNMENT Camera {
    float3 eye_ = make_float3(0.0f);
    float3 pixel00_relative_ = make_float3(0.0f);  // Precomputed: pixel00 - eye
    float3 pixel_du_ = make_float3(0.0f);
    float3 pixel_dv_ = make_float3(0.0f);

    Camera() = default;
    Camera(const Camera&) = default;
    Camera& operator=(const Camera&) = default;

#ifdef DEVICE
    // Device-only: helper for computing ray direction
    __device__ __forceinline__ float3 ray_direction(float2 jittered_pixel) const {
        // Compute: pixel00_relative + x*pixel_du + y*pixel_dv
        // Using nested FMA for optimal performance (6 fused multiply-add instructions):
        //   Inner FMA: y*pixel_dv + pixel00_relative
        //   Outer FMA: x*pixel_du + (result from inner)
        return common::math::fma(jittered_pixel.x, pixel_du_,
                                 common::math::fma(jittered_pixel.y, pixel_dv_, pixel00_relative_));
    }

    // Device-only: generate jittered camera ray
    __device__ __forceinline__ geometry::Ray jittered_ray(uint2 pixel, float2 jitter) const {
        const auto p = make_float2(pixel.x + jitter.x, pixel.y + jitter.y);
        const auto dir = ray_direction(p);
        return geometry::Ray::spawn(eye_, dir);
    }

    // Device-only: get camera position
    __device__ __forceinline__ float3 position() const { return eye_; }
#endif  // DEVICE
};

}  // namespace params
}  // namespace device
}  // namespace thesis
