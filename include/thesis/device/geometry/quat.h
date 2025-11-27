#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <sutil/vec_math.h>

namespace thesis {
namespace device {
namespace geometry {

struct UnitQuaternion {
    float s_;   // scalar part: w
    float3 u_;  // vector part: x, y, z

    UnitQuaternion() = default;
    UnitQuaternion(const UnitQuaternion&) = default;
    UnitQuaternion& operator=(const UnitQuaternion&) = default;

    UnitQuaternion(float w, float x, float y, float z, bool conj = false) : s_(w) {
        const auto sign = 1.0f - 2.0f * static_cast<float>(conj);  // +1.0 if false, -1.0 if true
        u_ = sign * make_float3(x, y, z);
    }

#ifdef DEVICE
    __device__ float3 rotate(float3 v) const {
        const auto uv = cross(u_, v);
        const auto uuv = cross(u_, uv);
        // Quaternion rotation: v + 2*s*(u×v) + 2*(u×(u×v))
        // = v + 2*(s*uv + uuv)
        return make_float3(__fmaf_rn(2.0f, __fmaf_rn(s_, uv.x, uuv.x), v.x),
                           __fmaf_rn(2.0f, __fmaf_rn(s_, uv.y, uuv.y), v.y),
                           __fmaf_rn(2.0f, __fmaf_rn(s_, uv.z, uuv.z), v.z));
    }
#endif  // DEVICE
};

}  // namespace geometry
}  // namespace device
}  // namespace thesis
