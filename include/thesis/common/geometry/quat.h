#pragma once

#include "thesis/common/utils/math.h"
#include "thesis/common/utils/preprocessor.h"

#include <cmath>

namespace thesis::common::geometry {

struct UnitQuaternion {
    float s_;   // scalar part: w
    float3 u_;  // vector part: x, y, z

   public:
    // Default: identity quaternion (no rotation)
    UnitQuaternion() : s_(1.0f), u_(make_float3(0.0f, 0.0f, 0.0f)) {}

   private:
    // Construct from normalized quaternion components (unchecked)
    THESIS_HOST_DEVICE THESIS_INLINE UnitQuaternion(float w, float x, float y, float z)
        : s_(w),
          u_(make_float3(x, y, z)) {}

   public:
    UnitQuaternion(const UnitQuaternion&) = default;
    UnitQuaternion& operator=(const UnitQuaternion&) = default;
    UnitQuaternion(UnitQuaternion&&) = default;
    UnitQuaternion& operator=(UnitQuaternion&&) = default;

    // Identity quaternion (no rotation)
    static THESIS_HOST_DEVICE THESIS_INLINE UnitQuaternion identity() {
        return {1.0f, 0.0f, 0.0f, 0.0f};
    }

    // Construct from normalized quaternion components (unchecked - assumes already normalized)
    static THESIS_HOST_DEVICE THESIS_INLINE UnitQuaternion from_unchecked(float w, float x, float y,
                                                                          float z) {
        return {w, x, y, z};
    }

    // Construct conjugate from normalized quaternion components (unchecked - negates vector part)
    static THESIS_HOST_DEVICE THESIS_INLINE UnitQuaternion conjugate(float w, float x, float y,
                                                                     float z) {
        return {w, -x, -y, -z};
    }

    // Get conjugate of this quaternion (inverse rotation)
    [[nodiscard]] THESIS_HOST_DEVICE THESIS_INLINE UnitQuaternion conjugate() const {
        return conjugate(s_, u_.x, u_.y, u_.z);
    }

    // Normalize quaternion components and construct
    static UnitQuaternion from(float w, float x, float y, float z) {
        const auto r = math::rlength(make_float4(w, x, y, z));
        return UnitQuaternion(w * r, x * r, y * r, z * r);
    }

    // Normalize quaternion components and construct conjugate
    static UnitQuaternion from_conjugate(float w, float x, float y, float z) {
        const auto r = math::rlength(make_float4(w, x, y, z));
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
