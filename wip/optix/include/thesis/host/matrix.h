#pragma once

#include "thesis/device/matrix.h"

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

device::Matrix3x4 toDevice(const glm::mat4& in) {
    device::Matrix3x4 out;
    // clang-format off
    out[0][0] = in[0][0]; out[0][1] = in[1][0]; out[0][2] = in[2][0]; out[0][3] = in[3][0];
    out[1][0] = in[0][1]; out[1][1] = in[1][1]; out[1][2] = in[2][1]; out[1][3] = in[3][1];
    out[2][0] = in[0][2]; out[2][1] = in[1][2]; out[2][2] = in[2][2]; out[2][3] = in[3][2];
    return out;
}

} // namespace host
} // namespace thesis

// __CUDACC__
