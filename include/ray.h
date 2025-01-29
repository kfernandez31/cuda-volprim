#pragma once

#include "vec.h"

#include <glm/gtx/compatibility.hpp>

class Ray {
public:
    glm::vec3 origin, direction;

    Ray(const glm::vec3& _origin, const glm::vec3& _direction, bool normalize=true)
        : origin(_origin), direction(normalize ? glm::normalize(_direction) : _direction) {}

    inline glm::vec3 at(float t) const {
        return origin + t * direction;
    }

    Ray in_coordinate_system(const glm::mat4& transformation_matrix) const {
        return Ray(
            transformation_matrix * glm::vec4(origin, 1.0),
            transformation_matrix * glm::vec4(direction, 0.0),
            false
        );
    }
};
