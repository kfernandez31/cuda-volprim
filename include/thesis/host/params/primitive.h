#pragma once

#include "thesis/common/utils/math.h"
#include "thesis/device/geometry/quat.h"
#include "thesis/device/params/primitive.h"
#include "thesis/host/params/convertible.h"
#include "thesis/host/utils/data.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/optimum_pow.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

namespace thesis::host::params {

class Primitive : public Convertible<device::params::Primitive> {
   private:
    glm::mat4 M_for_intersecting_;
    glm::vec3 S_diag_, S_diag_squared_;
    float S_det_;

    glm::vec3 center_;
    glm::quat rot_quat_;
    glm::vec3 scale_;

    glm::vec3 albedo_;
    float optical_depth_scale_;

   public:
    Primitive() = delete;

    Primitive(Primitive&&) noexcept = default;
    Primitive& operator=(Primitive&&) noexcept = default;

    Primitive(const Primitive&) = default;
    Primitive& operator=(const Primitive&) = default;

    // clang-format off
    Primitive(
        glm::vec3 center, const glm::quat& rot_quat, glm::vec3 scale,
        glm::vec3 albedo,
        float optical_depth_scale
    ) : S_diag_(scale),
        S_diag_squared_(glm::pow2(S_diag_)),
        S_det_(glm::compMul(S_diag_)),
        albedo_(albedo),
        optical_depth_scale_(optical_depth_scale),
        center_(center),
        rot_quat_(rot_quat),
        scale_(scale) {}

    [[nodiscard]] glm::mat4 localToWorld() const noexcept {
        static constexpr auto INTERSECTION_SCALING_FACTOR = 3.0f;

        const auto T = glm::translate(center_);
        const auto R = glm::toMat4(rot_quat_);
        const auto S = glm::scale(glm::vec3(scale_ * INTERSECTION_SCALING_FACTOR));

        return T * R * S;
    }

    [[nodiscard]] device::params::Primitive toDevice() const noexcept override {
        // clang-format off
        return device::params::Primitive(
            utils::data::toFloat3(S_diag_squared_),
            S_det_,
            utils::data::toFloat3(albedo_),
            optical_depth_scale_,
            common::math::ONE_OVER_TWO_ROOT_TWO_F * glm::inversesqrt(S_det_),
            utils::data::toFloat3(center_),
            device::geometry::UnitQuaternion(rot_quat_.x, rot_quat_.y, rot_quat_.z, rot_quat_.w, true),
            utils::data::toFloat3(scale_)
        );
    }
};

}  // namespace thesis::host::params
