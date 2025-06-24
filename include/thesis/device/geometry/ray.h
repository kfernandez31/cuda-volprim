#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <vector_types.h>

#ifdef __CUDACC__
#include "thesis/common/utils/math.h"
#include "thesis/device/geometry/matrix.h"

#include <optix.h>

#include <sutil/vec_math.h>
#endif  // __CUDACC__

namespace thesis {
namespace device {
namespace geometry {

class THESIS_ALIGNMENT Ray {
   private:
    __device__ Ray(float3 origin, float3 direction) : origin_(origin), direction_(direction) {}

   public:
    float3 origin_;
    float3 direction_;

#ifdef __CUDACC__
    enum Type {
        RADIANCE = 0,
        SHADOW = 1,
        REFLECTION = 2,
        COUNT = 3,
    };

    static __forceinline__ __device__ Ray spawn(float3 o, float3 d) noexcept {
        return {o, normalize(d)};
    }

    static __forceinline__ __device__ Ray spawn_unchecked(float3 o, float3 d) noexcept {
        return {o, d};  // assume caller normalized
    }

    static __forceinline__ __device__ Ray getCurrentRay() noexcept {
        return spawn_unchecked(optixGetWorldRayOrigin(), optixGetWorldRayDirection());
    }

    __forceinline__ __device__ bool is_normalized() noexcept {
        return fabsf(common::math::length2(direction_) - 1.0f) < 1e-4f;
    }

    __forceinline__ __device__ float3 at(float t) const noexcept {
        return origin_ + t * direction_;
    }

    __forceinline__ __device__ Ray transformed(const Matrix3x4& mat) const noexcept {
        return spawn(mat.transform<true>(origin_), mat.transform<false>(direction_));
    }
#endif  // __CUDACC__
};

}  // namespace geometry
}  // namespace device
}  // namespace thesis