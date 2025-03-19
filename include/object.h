#pragma once

#include "interval.h"
#include "hit_record.h"
#include "mat.h"
#include "mesh.h"
#include "ray.h"

#include "dbg.h"
#include <glm/geometric.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/optimum_pow.hpp>

#include <atomic>
#include <algorithm>
#include <memory>
#include <optional>

using ObjectId = size_t;

class Object : public std::enable_shared_from_this<Object> {
private:
    ObjectId getNextId() {
        return nextId.fetch_add(1, std::memory_order_relaxed);
    }

    virtual float density_integral_impl(const Ray& r, const Interval& t_range) const {
        return math::inf<float>();
    }
public:
    static std::atomic<ObjectId> nextId;
    ObjectId id;

    mat4 T, R, S; // TODO: last row is redundant, 3x4 would be better
    mat4 M, M_for_integrating, M_for_intersecting;
    mat4 M_inv, M_for_integrating_inv, M_for_intersecting_inv;
    vec3 S_diag, SS;
    float density_integral_erf_denominator_base;

    static constexpr const float intersection_scaling_factor = 3.0f;

    mat4 get_M() {
        return T * R * S;
    }

    mat4 get_M_for_integrating() {
        return T * R;
    }

    mat4 get_M_for_intersecting() {
        mat4 result = M;
        set_diagonal(result, get_diagonal(result) * intersection_scaling_factor);
        return result;
    }

    mat4 get_M_inv() const {
        auto S_inv = glm::scale(1.0f / get_diagonal(S));
        auto R_inv = glm::transpose(R);
        auto T_inv = glm::translate(vec3(-T[3]));
        return S_inv * R_inv * T_inv;
    }

    mat4 get_M_for_integrating_inv() const {
        auto R_inv = glm::transpose(R);
        auto T_inv = glm::translate(vec3(-T[3]));
        return R_inv * T_inv;
    }

    mat4 get_M_for_intersecting_inv() const {
        auto S_inv = glm::scale(1.0f / (intersection_scaling_factor * get_diagonal(S)));
        auto R_inv = glm::transpose(R);
        auto T_inv = glm::translate(vec3(-T[3]));
        return S_inv * R_inv * T_inv;
    }
public:
    vec3 albedo;
    float optical_depth_scale;

    struct RotationData {
        float angle;
        vec3 rotation_axis;

        RotationData(float _angle, const vec3& _rotation_axis)
            : angle(glm::radians(glm::mod(_angle, 360.0f))), rotation_axis(_rotation_axis) {}

        inline mat4 to_rotation_matrix() const {
            return glm::abs(angle) < 1e-8 ? glm::identity<mat4>() : glm::rotate(angle, rotation_axis);
        }
    };

    static const RotationData NoRotation;

    // TODO: a ctor from a normalized quaternion for R
    Object(const vec3& _albedo, float _optical_depth_scale, const mat4& _T, const mat4& _R, const mat4& _S)
        : id(getNextId())
        , T(_T), R(_R), S(_S)
        , M(get_M())
        , M_for_integrating(get_M_for_integrating())
        , M_for_intersecting(get_M_for_intersecting())
        , M_inv(get_M_inv())
        , M_for_integrating_inv(get_M_for_integrating_inv())
        , M_for_intersecting_inv(get_M_for_intersecting_inv())
        , S_diag(get_diagonal(S))
        , SS(glm::pow2(S_diag))
        , density_integral_erf_denominator_base(glm::inversesqrt(glm::compMul(S_diag)) * glm::one_over_root_two<float>())
        , albedo(_albedo)
        , optical_depth_scale(_optical_depth_scale)
        {}

    Object() = default;
    virtual ~Object() = default;

    virtual std::optional<HitRecord> intersect(const Ray& r, float t_min=0.0f) = 0;

    float density_integral(const Ray& r, const Interval& t_range) const {
        return optical_depth_scale * density_integral_impl(r, t_range);
    }

    template <typename Vec4>
    std::vector<Vec4> transform(const Mesh& mesh) const {
        std::vector<Vec4> transformed_vertices;
        transformed_vertices.reserve(mesh.vertices.size());

        // TODO: this'd be more efficient if it was just GEMM
        for (const auto& v : mesh.vertices) {
            auto v_transformed = M_for_intersecting * glm::vec4(v, 1.0f);
            transformed_vertices.emplace_back(v_transformed.x, v_transformed.y, v_transformed.z, 0);
        }

        std::vector<Vec4> ordered_vertices;
        ordered_vertices.reserve(mesh.vertices.size());

        for (const auto& tri : mesh.triangles) {
            ordered_vertices.insert(ordered_vertices.end(), {
                transformed_vertices[tri[0]],
                transformed_vertices[tri[1]],
                transformed_vertices[tri[2]],
            });
        }

        return ordered_vertices;
    }

};

const Object::RotationData Object::NoRotation(0, vec3(0));
std::atomic<ObjectId> Object::nextId = 0;
