#pragma once

#include "ellipsoid.h"

class Sphere : public Ellipsoid {
public:
    Sphere(const glm::vec3& _color, float _transmittance_scale, const glm::vec3& _center, float _radius)
        : Ellipsoid(_color, _transmittance_scale, _center, glm::vec3(_radius)) {}
};
