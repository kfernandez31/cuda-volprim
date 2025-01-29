#pragma once

#include "object.h"
#include "vec.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/optimum_pow.hpp>

class Ellipsoid : public Object {
public:
    Ellipsoid(const glm::vec3& _color, float _transmittance_scale, const glm::vec3& _center, const glm::vec3& _semi_axes_lengths, RotationData rot=NoRotation)
        : Object(_color, _transmittance_scale, glm::translate(_center), rot.to_rotation_matrix(), glm::scale(_semi_axes_lengths)) {}

    using Object::Object;

    virtual ~Ellipsoid() = default;

    std::optional<HitRecord> intersect(const Ray& r_global, const Interval& t_range) override {
        auto r_local = r_global.in_coordinate_system(M_inv);

        // Coefficients
        auto a = glm::length2(r_local.direction);
        auto a_inv = 1.0f / a;
        auto b = -glm::dot(r_local.origin, r_local.direction);
        auto c = glm::length2(r_local.origin) - 1.0f;

        auto discriminant = 1.0f - glm::length2(r_local.at(b * a_inv));
        if (discriminant < 0.0f)
            return {};

        // Roots
        auto q = b + glm::sign(b) * glm::sqrt(a * discriminant);
        auto root_1 = c / q;
        auto root_2 = q * a_inv;

        if (root_2 < 0.0f || glm::epsilonEqual(root_1, root_2, 1e-8f)) // TODO: possibly tweak epsilon
            return {};
        return HitRecord(shared_from_this(), glm::max(0.0f, root_1), root_2);
    }
};
