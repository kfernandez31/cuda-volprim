#pragma once

#include "vec.h"

#include <glm/mat4x4.hpp>

using glm::mat3;
using glm::mat4;

std::ostream& operator<<(std::ostream& os, const glm::mat3& mat) {
    os << "[";
    for (int i = 0; i < 3; ++i) {
        os << "(" << mat[0][i] << ", " << mat[1][i] << ", " << mat[2][i] << ")";
        if (i < 2) os << "\n ";
    }
    os << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, const glm::mat4& mat) {
    os << "[";
    for (int i = 0; i < 4; ++i) {
        os << "(" << mat[0][i] << ", " << mat[1][i] << ", " << mat[2][i] << ", " << mat[3][i] << ")";
        if (i < 3) os << "\n ";
    }
    os << "]";
    return os;
}

inline vec3 get_diagonal(const mat4& M) {
    return {M[0][0], M[1][1], M[2][2]};
}

inline void set_diagonal(mat4& M, const vec3& v) {
    M[0][0] = v[0];
    M[1][1] = v[1];
    M[2][2] = v[2];
}

glm::mat3 quat2rotmat(const glm::quat& q) {
    assert(glm::epsilonEqual(glm::length(q), 1.0f, 1e-8f)); // the quaternion has to be normalized
    float w = q.w, x = q.x, y = q.y, z = q.z;

    return glm::mat3(
        1.0f - 2.0f * (y * y + z * z),  2.0f * (x * y - w * z),         2.0f * (x * z + w * y),
        2.0f * (x * y + w * z),         1.0f - 2.0f * (x * x + z * z),  2.0f * (y * z - w * x),
        2.0f * (x * z - w * y),         2.0f * (y * z + w * x),         1.0f - 2.0f * (x * x + y * y)
    );
}
