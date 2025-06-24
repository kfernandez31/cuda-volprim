#pragma once

#include "thesis/device/geometry/matrix.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/optimum_pow.hpp>

namespace thesis::host::geometry {

inline glm::vec3 getDiagonal(const glm::mat4& mat) noexcept {
    return {mat[0][0], mat[1][1], mat[2][2]};
}

inline void scaleDiagonal(glm::mat4& mat, float s) noexcept {
    mat[0][0] *= s;
    mat[1][1] *= s;
    mat[2][2] *= s;
}

inline device::geometry::Matrix3x4 toDevice(const glm::mat4& in) noexcept {
    device::geometry::Matrix3x4 out;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            out[i][j] = in[j][i];  // transpose, since glm is col-major
        }
    }
    return out;
}

}  // namespace thesis::host::geometry
