#pragma once

#include "thesis/device/geometry/matrix.h"

#include <glm/glm.hpp>

namespace thesis {
namespace host {

inline glm::vec3 getDiagonal(const glm::mat4& mat) noexcept {
    return {mat[0][0], mat[1][1], mat[2][2]};
}

inline void scaleDiagonal(glm::mat4& mat, float s) noexcept {
    mat[0][0] *= s;
    mat[1][1] *= s;
    mat[2][2] *= s;
}

inline device::Matrix3x4 toDevice(const glm::mat4& in) noexcept {
    device::Matrix3x4 out;
    // clang-format off
    out[0][0] = in[0][0]; out[0][1] = in[1][0]; out[0][2] = in[2][0]; out[0][3] = in[3][0];
    out[1][0] = in[0][1]; out[1][1] = in[1][1]; out[1][2] = in[2][1]; out[1][3] = in[3][1];
    out[2][0] = in[0][2]; out[2][1] = in[1][2]; out[2][2] = in[2][2]; out[2][3] = in[3][2];
    return out;
}

inline glm::mat4 rotationMatrixFromNormalizedQuaternion(const glm::quat& q) noexcept {
    const auto x = q.x, y = q.y, z = q.z, w = q.w;

    const auto xx = glm::pow2(x);
    const auto yy = glm::pow2(y);
    const auto zz = glm::pow2(z);
    const auto xy = x * y;
    const auto xz = x * z;
    const auto yz = y * z;
    const auto wx = w * x;
    const auto wy = w * y;
    const auto wz = w * z;

    // clang-format off
    glm::mat4 result = {
        1.0f - 2.0f * (yy + zz),  2.0f * (xy + wz),        2.0f * (xz - wy),        0.0f,
        2.0f * (xy - wz),         1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx),        0.0f,
        2.0f * (xz + wy),         2.0f * (yz - wx),        1.0f - 2.0f * (xx + yy), 0.0f,
        0.0f,                     0.0f,                    0.0f,                    1.0f
    };

    return result;
}

} // namespace host
} // namespace thesis

// __CUDACC__
