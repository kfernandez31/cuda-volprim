#pragma once

#include "thesis/common/utils/math.h"
#include "thesis/common/utils/preprocessor.h"

#include <cmath>

namespace thesis::common::geometry {

struct UnitQuaternion {
    float s_;   // scalar part: w
    float3 u_;  // vector part: x, y, z

    UnitQuaternion() = default;
    UnitQuaternion(const UnitQuaternion&) = default;
    UnitQuaternion& operator=(const UnitQuaternion&) = default;
    UnitQuaternion(UnitQuaternion&&) = default;
    UnitQuaternion& operator=(UnitQuaternion&&) = default;

    // Construct from normalized quaternion components
    THESIS_HOST_DEVICE THESIS_INLINE UnitQuaternion(float w, float x, float y, float z)
        : s_(w),
          u_(make_float3(x, y, z)) {}

    // Construct conjugate from normalized quaternion components (negates vector part)
    static THESIS_HOST_DEVICE THESIS_INLINE UnitQuaternion conjugate(float w, float x, float y,
                                                                     float z) {
        return {w, -x, -y, -z};
    }

    // Normalize quaternion components and construct
    static UnitQuaternion from_unnormalized(float w, float x, float y, float z) {
        const auto r =
            math::rsqrt(math::fma(w, w, math::length2(make_float3(x, y, z))));  // 1 over length
        return UnitQuaternion(w * r, x * r, y * r, z * r);
    }

    // Normalize quaternion components and construct conjugate
    static UnitQuaternion from_unnormalized_conjugate(float w, float x, float y, float z) {
        const auto r =
            math::rsqrt(math::fma(w, w, math::length2(make_float3(x, y, z))));  // 1 over length
        return conjugate(w * r, x * r, y * r, z * r);
    }

    // Quaternion rotation: v' = v + 2*s*(u×v) + 2*(u×(u×v))
    THESIS_HOST_DEVICE THESIS_INLINE float3 rotate(float3 v) const {
        const auto uv = math::cross(u_, v);
        const auto uuv = math::cross(u_, uv);
        return math::fma(2.0f, math::fma(s_, uv, uuv), v);
    }
};

}  // namespace thesis::common::geometry
