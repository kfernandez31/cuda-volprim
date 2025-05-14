#pragma once

#include "thesis/utils/preprocessor.h"

#include <vector_types.h>

#ifdef __CUDACC__
#include "thesis/device/matrix.h"

#include <optix.h>

#include <sutil/vec_math.h>
#endif  // __CUDACC__

namespace thesis {
namespace device {

struct THESIS_ALIGNMENT Ray {
    float3 origin_;
    float3 direction_;

    Ray() = default;
    THESIS_HOST_DEVICE Ray(float3 origin, float3 direction)
        : origin_(origin), direction_(direction) {}

#ifdef __CUDACC__
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
        return {transform<true>(mat, origin_), transform<false>(mat, direction_)};
    }
#endif  // __CUDACC__
};

}  // namespace device
}  // namespace thesis
