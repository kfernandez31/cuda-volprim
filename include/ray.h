#pragma once

#include "vec.h"

#include <glm/gtx/compatibility.hpp>

class Ray {
public:
    vec3 origin, direction;

    Ray(const vec3& _origin, const vec3& _direction, bool normalize=true)
        : origin(_origin), direction(normalize ? glm::normalize(_direction) : _direction) {}

    Ray(const Ray& other)
        : origin(other.origin), direction(other.direction) {}

    Ray& operator=(const Ray& other) {
        if (this != &other) {
            origin = other.origin;
            direction = other.direction;
        }
        return *this;
    }

    inline vec3 at(float t) const {
        return origin + t * direction;
    }

    inline Ray advanced_by(float t) const {
        return Ray(origin + t * direction, direction);
    }

    inline void march_by(float t, float offset=1e-8) {
        origin = at(t + offset);
    }

    Ray in_coordinate_system(const mat4& M) const {
        return Ray(M * vec4(origin, 1.0), M * vec4(direction, 0.0), false);
    }
};
