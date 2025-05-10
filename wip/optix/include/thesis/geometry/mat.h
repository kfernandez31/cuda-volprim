#pragma once

#include <glm/glm.hpp>

namespace thesis::geometry {

inline glm::vec3 get_diagonal(const glm::mat4& M) {
    return {M[0][0], M[1][1], M[2][2]};
}

inline void set_diagonal(glm::mat4& M, const glm::vec3& v) {
    M[0][0] = v[0];
    M[1][1] = v[1];
    M[2][2] = v[2];
}

/* TODO(kacper): remove?
glm::mat3 quat2rotmat(const glm::quat& q) {
    assert(glm::epsilonEqual(glm::length(q), 1.0f, 1e-8f)); // the quaternion has to be normalized
    float w = q.w, x = q.x, y = q.y, z = q.z;

    return glm::mat3(
        1.0f - 2.0f * (y * y + z * z),  2.0f * (x * y - w * z),         2.0f * (x * z + w * y),
        2.0f * (x * y + w * z),         1.0f - 2.0f * (x * x + z * z),  2.0f * (y * z - w * x),
        2.0f * (x * z - w * y),         2.0f * (y * z + w * x),         1.0f - 2.0f * (x * x + y * y)
    );
}
*/

} // namespace thesis::geometry
