#pragma once

#include "thesis/common/geometry/quat.h"
#include "thesis/common/utils/math.h"

#include <cuda_runtime.h>
#include <vector_types.h>

#include <cassert>
#include <initializer_list>
#include <span>

namespace thesis::host::utils::math {

// Row-major 3x4 transformation matrix for OptiX
// Layout: 3 rows × 4 columns, translation in the 4th column
// [R00*sx  R01*sy  R02*sz  tx]
// [R10*sx  R11*sy  R12*sz  ty]
// [R20*sx  R21*sy  R22*sz  tz]
struct Mat3x4 {
    float data[12];  // 3 rows × 4 columns

    Mat3x4() = default;

    Mat3x4(std::initializer_list<float> values) {
        assert(values.size() == 12 && "Mat3x4 requires exactly 12 values");
        std::copy(values.begin(), values.end(), data);
    }

    explicit Mat3x4(std::span<const float, 12> values) {
        std::copy(values.begin(), values.end(), data);
    }

    float* operator[](size_t row) { return &data[row * 4]; }

    const float* operator[](size_t row) const { return &data[row * 4]; }

    const float* ptr() const { return data; }

    // Create TRS transformation matrix directly from translation, rotation (quaternion), and scale
    // M = T * R * S, where:
    // - T is translation (4th column)
    // - R is rotation matrix from normalized quaternion
    // - S is scale (diagonal matrix)
    //
    // Quaternion to rotation matrix formula (for normalized quaternion q = [w, x, y, z]):
    // R = [ 1-2(y²+z²)   2(xy-wz)     2(xz+wy)   ]
    //     [ 2(xy+wz)     1-2(x²+z²)   2(yz-wx)   ]
    //     [ 2(xz-wy)     2(yz+wx)     1-2(x²+y²) ]
    static Mat3x4 from_trs(float3 translation, const common::geometry::UnitQuaternion& q,
                           float3 scale) {
        using namespace common::math;

        Mat3x4 result;

        // Extract quaternion components
        const float qw = q.s_;
        const float qx = q.u_.x;
        const float qy = q.u_.y;
        const float qz = q.u_.z;

        // Compute squared components (common terms)
        const float xx = pow2(qx);
        const float yy = pow2(qy);
        const float zz = pow2(qz);

        // Compute cross-term products
        const float xy = qx * qy;
        const float xz = qx * qz;
        const float yz = qy * qz;
        const float wx = qw * qx;
        const float wy = qw * qy;
        const float wz = qw * qz;

        // Row 0: First row of R (scaled) + translation.x
        // Use FMA for (1 - 2*(yy+zz))*sx = fma(-2, yy+zz, 1)*sx
        result[0][0] = fma(-2.0f, yy + zz, 1.0f) * scale.x;
        result[0][1] = 2.0f * (xy - wz) * scale.y;
        result[0][2] = 2.0f * (xz + wy) * scale.z;
        result[0][3] = translation.x;

        // Row 1: Second row of R (scaled) + translation.y
        result[1][0] = 2.0f * (xy + wz) * scale.x;
        result[1][1] = fma(-2.0f, xx + zz, 1.0f) * scale.y;
        result[1][2] = 2.0f * (yz - wx) * scale.z;
        result[1][3] = translation.y;

        // Row 2: Third row of R (scaled) + translation.z
        result[2][0] = 2.0f * (xz - wy) * scale.x;
        result[2][1] = 2.0f * (yz + wx) * scale.y;
        result[2][2] = fma(-2.0f, xx + yy, 1.0f) * scale.z;
        result[2][3] = translation.z;

        return result;
    }
};

}  // namespace thesis::host::utils::math
