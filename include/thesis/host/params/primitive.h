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
    glm::vec3 center_;
    glm::quat rot_quat_;
    glm::vec3 scale_;
    glm::vec3 albedo_;
    float optical_thickness_;

   public:
    Primitive() = delete;
    Primitive(const Primitive&) = default;
    Primitive& operator=(const Primitive&) = default;

    Primitive(glm::vec3 center, const glm::quat& rot_quat, glm::vec3 scale, glm::vec3 albedo,
              float optical_thickness)
        : scale_(scale),
          rot_quat_(glm::normalize(rot_quat)),
          center_(center),
          albedo_(albedo),
          optical_thickness_(optical_thickness) {}

    [[nodiscard]] glm::mat4 localToWorld() const noexcept {
        static constexpr auto INTERSECTION_SCALING_FACTOR = 1.0f;  // TODO(kacper): set to 3.0f

        const auto T = glm::translate(center_);
        const auto R = glm::toMat4(rot_quat_);
        const auto S = glm::scale(glm::vec3(scale_ * INTERSECTION_SCALING_FACTOR));

        return T * R * S;
    }

    [[nodiscard]] device::params::Primitive toDevice() const noexcept override {
        // clang-format off
        return device::params::Primitive(
            utils::data::toFloat3(center_),
            device::geometry::UnitQuaternion(rot_quat_.w, rot_quat_.x, rot_quat_.y, rot_quat_.z, true),
            utils::data::toFloat3(scale_),
            utils::data::toFloat3(albedo_),
            optical_thickness_
        );
    }
};

}  // namespace thesis::host::params
