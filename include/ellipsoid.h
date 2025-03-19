#pragma once

#include "object.h"
#include "vec.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/optimum_pow.hpp>

class Ellipsoid : public Object {
protected:
    std::optional<HitRecord> intersect_impl(const Ray& r_global, const mat4& inv_trans_mat, float t_min) {
        auto r_local = r_global.in_coordinate_system(inv_trans_mat);

        // Coefficients
        auto a     = glm::length2(r_local.direction);
        auto a_inv = 1.0f / a;
        auto b     = -glm::dot(r_local.origin, r_local.direction);

        auto discriminant = 1.0f - glm::length2(r_local.at(b * a_inv));
        if (discriminant < 0.0f)
            return {};

        // Roots
        auto c = glm::length2(r_local.origin) - 1.0f;
        auto q = b + glm::sign(b) * glm::sqrt(a * discriminant);

        auto t_2 = q * a_inv;
        if (t_2 < t_min)
            return {};

        auto t_1 = c / q;
        if (glm::epsilonEqual(t_1, t_2, 1e-8f))
            return {};

        return HitRecord(shared_from_this(), glm::max(t_min, t_1), t_2);
    }
public:
    Ellipsoid(const vec3& _albedo, float _optical_depth_scale, const vec3& _center, RotationData rot, const vec3& _semi_axes_lengths)
        : Object(_albedo, _optical_depth_scale, glm::translate(_center), rot.to_rotation_matrix(), glm::scale(_semi_axes_lengths)) {}

    using Object::Object;

    virtual ~Ellipsoid() = default;

    std::optional<HitRecord> intersect(const Ray& r_global, float t_min=0.0f) override {
        return intersect_impl(r_global, M_inv, t_min);
    }
};
