#pragma once

#include "ellipsoid.h"

class Sphere : public Ellipsoid {
public:
    using Ellipsoid::Ellipsoid;

    Sphere(const vec3& _albedo, float _optical_depth_scale, const vec3& _center, float _radius)
        : Ellipsoid(_albedo, _optical_depth_scale, _center, Object::NoRotation, vec3(_radius)) {}

    // TODO: what does this do? Should I use it?
    static void get_sphere_uv(const vec3& p, float& u, float& v) {
        // p: a given point on the sphere of radius one, centered at the origin.
        // u: returned value [0,1] of angle around the Y axis from X=-1.
        // v: returned value [0,1] of angle from Y=-1 to Y=+1.
        //     <1 0 0> yields <0.50 0.50>       <-1  0  0> yields <0.00 0.50>
        //     <0 1 0> yields <0.50 1.00>       < 0 -1  0> yields <0.50 0.00>
        //     <0 0 1> yields <0.25 0.50>       < 0  0 -1> yields <0.75 0.50>

        auto theta = glm::acos(-p[1]);
        auto phi = glm::atan2(-p[2], p[0]) + glm::pi<float>();

        u = phi * glm::one_over_two_pi<float>();
        v = theta * glm::one_over_pi<float>();
    }
};
