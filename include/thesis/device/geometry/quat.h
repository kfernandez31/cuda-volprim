#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <sutil/vec_math.h>

namespace thesis {
namespace device {
namespace geometry {

struct UnitQuaternion {
    float3 u_;  // vector part: x, y, z
    float s_;   // scalar part: w

    UnitQuaternion() = default;
    UnitQuaternion(const UnitQuaternion&) = default;
    UnitQuaternion(UnitQuaternion&&) = default;
    UnitQuaternion& operator=(const UnitQuaternion&) = default;
    UnitQuaternion& operator=(UnitQuaternion&&) = default;

    UnitQuaternion(float x, float y, float z, float w, bool conj = false) : s_(w) {
        const auto sign = 1.0f - 2.0f * static_cast<float>(conj);  // +1.0 if false, -1.0 if true
        u_ = sign * make_float3(x, y, z);
        s_ = w;
    }

    THESIS_INLINE THESIS_HOST_DEVICE float3 rotate(const float3& v) const {
        const auto uv = cross(u_, v);
        const auto uuv = cross(u_, uv);
        return v + 2.0f * (s_ * uv + uuv);
    }
};

}  // namespace geometry
}  // namespace device
}  // namespace thesis
