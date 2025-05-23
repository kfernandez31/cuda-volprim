#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <vector_types.h>

#ifdef __CUDACC__
#include "thesis/device/geometry/matrix.h"

#include <optix.h>

#include <sutil/vec_math.h>
#endif  // __CUDACC__

namespace thesis {
namespace device {

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
        return {optixGetWorldRayOrigin(), optixGetWorldRayDirection()};
    }

    __forceinline__ __device__ float3 at(float t) const noexcept {
        return origin_ + t * direction_;
    }

    __forceinline__ __device__ Ray advanced_by(float t) const noexcept {
        auto o = origin_ + t * direction_;
        auto d = normalize(direction_);
        return {o, d};
    }

    __forceinline__ __device__ void march_by(float t, float offset = 1e-8) noexcept {
        origin_ = at(t + offset);
    }

    __forceinline__ __device__ Ray transformed(const Matrix3x4& mat) const noexcept {
        return {Matrix3x4::transform<true>(mat, origin_),
                Matrix3x4::transform<false>(mat, direction_)};
    }
#endif  // __CUDACC__
};

}  // namespace device
}  // namespace thesis