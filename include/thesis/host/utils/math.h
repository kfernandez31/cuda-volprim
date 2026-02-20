#pragma once

#include "thesis/common/geometry/quat.h"
#include "thesis/common/utils/math.h"

#include <cuda_runtime.h>
#include <vector_types.h>

#include <cassert>
#include <cfloat>
#include <initializer_list>
#include <span>
#include <utility>

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

// Morton Code (Z-order curve) functions for spatial locality
// Expands a 10-bit integer into 30 bits by inserting 2 zeros between each bit
// Example: 0b1111 -> 0b1001001001
inline constexpr uint32_t expandBits(uint32_t v) noexcept {
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

// Encode 3D position to 30-bit Morton code (10 bits per dimension)
// Morton code interleaves x, y, z bits: z|y|x|z|y|x|...
// Points that are spatially close get numerically close Morton codes
inline uint32_t morton3D(float3 pos, float3 scene_min, float3 scene_max) noexcept {
    using namespace common::math;

    // Normalize position to [0, 1] range
    const float3 normalized = (pos - scene_min) / (scene_max - scene_min);

    // Clamp to [0, 1] and convert to 10-bit integers (0-1023)
    const uint32_t x = static_cast<uint32_t>(clamp(normalized.x, 0.0f, 1.0f) * 1023.0f);
    const uint32_t y = static_cast<uint32_t>(clamp(normalized.y, 0.0f, 1.0f) * 1023.0f);
    const uint32_t z = static_cast<uint32_t>(clamp(normalized.z, 0.0f, 1.0f) * 1023.0f);

    // Interleave bits: shift expanded components and OR them together
    const uint32_t xx = expandBits(x);
    const uint32_t yy = expandBits(y);
    const uint32_t zz = expandBits(z);

    return (zz << 2) | (yy << 1) | xx;
}

// Compute axis-aligned bounding box for a collection of primitives
template <typename PrimitiveContainer>
inline std::pair<float3, float3> computeBounds(const PrimitiveContainer& primitives) noexcept {
    using namespace common::math;

    if (primitives.empty()) {
        return {make_float3(0.0f), make_float3(0.0f)};
    }

    float3 scene_min = make_float3(FLT_MAX);
    float3 scene_max = make_float3(-FLT_MAX);

    for (const auto& prim : primitives) {
        const auto center = prim.center();

        // Bound only by center: Morton ordering is based on relative position,
        // including scale would compress quantization precision unnecessarily.
        scene_min.x = min(scene_min.x, center.x);
        scene_min.y = min(scene_min.y, center.y);
        scene_min.z = min(scene_min.z, center.z);

        scene_max.x = max(scene_max.x, center.x);
        scene_max.y = max(scene_max.y, center.y);
        scene_max.z = max(scene_max.z, center.z);
    }

    return {scene_min, scene_max};
}

}  // namespace thesis::host::utils::math
