#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <vector_types.h>

#ifdef DEVICE
#include "thesis/common/utils/math.h"

#include <optix.h>
#endif  // DEVICE

namespace thesis {
namespace device {
namespace geometry {

class THESIS_ALIGNMENT Ray {
#ifdef DEVICE
    THESIS_HOST_DEVICE THESIS_INLINE Ray(float3 origin, float3 direction)
        : origin_(origin),
          direction_(direction) {}
#endif  // DEVICE
   public:
    float3 origin_;
    float3 direction_;

    Ray(const Ray&) = default;
    Ray& operator=(const Ray&) = default;

    enum Type {
        DEFAULT = 0,
        COUNT = 1,
    };

#ifdef DEVICE
    static __device__ __forceinline__ Ray spawn(float3 o, float3 d) {
        return spawn_unchecked(o, common::math::normalize(d));
    }

    static __device__ __forceinline__ Ray spawn_unchecked(float3 o, float3 d) {
        return {o, d};  // assume caller normalized
    }

    static __device__ __forceinline__ Ray getCurrentRay() {
        return spawn_unchecked(optixGetWorldRayOrigin(), optixGetWorldRayDirection());
    }

    __device__ __forceinline__ float3 at(float t) const { return origin_ + t * direction_; }
#endif  // DEVICE
};

}  // namespace geometry
}  // namespace device
}  // namespace thesis