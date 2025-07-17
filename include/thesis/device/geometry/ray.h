#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <vector_types.h>

#ifdef DEVICE
#include "thesis/common/utils/math.h"

#include <optix.h>

#include <sutil/vec_math.h>
#endif  // DEVICE

namespace thesis {
namespace device {
namespace geometry {

class THESIS_ALIGNMENT Ray {
#ifdef DEVICE
    __host__ __device__ Ray(float3 origin, float3 direction)
        : origin_(origin), direction_(direction) {}
#endif  // DEVICE
   public:
    float3 origin_;
    float3 direction_;

    Ray(const Ray&) = default;
    Ray& operator=(const Ray&) = default;

    enum Type {
        RADIANCE = 0,
        SHADOW = 1,
        REFLECTION = 2,
        COUNT = 3,
    };

#ifdef DEVICE
    static __device__ Ray spawn(float3 o, float3 d) { return {o, normalize(d)}; }

    static __device__ Ray spawn_unchecked(float3 o, float3 d) {
        return {o, d};  // assume caller normalized
    }

    static __device__ Ray getCurrentRay() {
        return spawn_unchecked(optixGetWorldRayOrigin(), optixGetWorldRayDirection());
    }

    __device__ float3 at(float t) const { return origin_ + t * direction_; }
#endif  // DEVICE
};

}  // namespace geometry
}  // namespace device
}  // namespace thesis