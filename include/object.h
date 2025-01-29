#pragma once

#include "interval.h"
#include "hit_record.h"
#include "ray.h"

#include "dbg.h"
#include <glm/geometric.hpp>
#include <glm/gtx/transform.hpp>

#include <algorithm>
#include <memory>
#include <optional>

class Object : public std::enable_shared_from_this<Object> {
protected:
    glm::mat4 T, R, S, M_inv;
    glm::vec3 S_diagonal, M_diagonal;
    float M_det_inv;

    glm::mat4 get_M_inv() const {
        auto S_inv = glm::scale(1.0f / get_diagonal(S));
        auto R_inv = glm::transpose(R);
        auto T_inv = glm::translate(glm::vec3(-T[3]));
        return S_inv * R_inv * T_inv;
    }
public:
    glm::vec3 color;
    float transmittance_scale;

    struct RotationData {
        float angle;
        glm::vec3 rotation_axis;

        RotationData(float _angle, const glm::vec3& _rotation_axis)
            : angle(glm::radians(glm::mod(_angle, 360.0f))), rotation_axis(_rotation_axis) {}

        inline glm::mat4 to_rotation_matrix() const {
            return glm::abs(angle) < 1e-8 ? glm::identity<glm::mat4>() : glm::rotate(angle, rotation_axis);
        }
    };

    static const RotationData NoRotation;

    Object(const glm::vec3& _color, float _transmittance_scale, const glm::mat4& _T, const glm::mat4& _R, const glm::mat4& _S)
        : T(_T), R(_R), S(_S)
        , M_inv(get_M_inv())
        , S_diagonal(get_diagonal(S))
        , M_diagonal(get_diagonal(R) * S_diagonal)
        , M_det_inv(glm::inversesqrt(2.0f * glm::determinant(R) * glm::compMul(S_diagonal))) // TODO: avoid computing the determinant of R
        , color(_color)
        , transmittance_scale(_transmittance_scale)
        {}

    Object() {}

    // TODO: is the t_range parameter necessary?
    virtual std::optional<HitRecord> intersect(const Ray& r, const Interval& t_range) = 0;

    // TODO:
    virtual float transmittance(const Ray& r, const Interval& t) const {
        return 0.0;
    }
};

const Object::RotationData Object::NoRotation(0.0f, glm::vec3(0));

