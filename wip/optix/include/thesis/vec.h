#pragma once

#include <glm/glm.hpp>
#include <vector_types.h>

namespace thesis {

inline float3 to_float3(const glm::vec3& v) {
    return make_float3(v.x, v.y, v.z);
}

inline glm::vec3 to_vec3(const float3& v) {
    return glm::vec3(v.x, v.y, v.z);
}

} // namespace thesis
