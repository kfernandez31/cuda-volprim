#pragma once

#include "thesis/common/utils/math.h"
#include "thesis/device/params/primitive.h"
#include "thesis/host/geometry/matrix.h"
#include "thesis/host/params/convertible.h"
#include "thesis/host/utils/data.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/optimum_pow.hpp>
#include <glm/gtx/transform.hpp>

namespace thesis {
namespace host {

class Primitive : public Convertible<device::Primitive> {
   private:
    glm::mat4 M_for_integrating_inv_;
    glm::vec3 S_diag_, S_diag_squared_;
    glm::vec3 albedo_;
    float optical_depth_scale_;

    // TODO(kacper): account for this while creating an icosphere
    // static constexpr auto INTERSECTION_SCALING_FACTOR = 3.0f;

    static inline glm::mat4 get_M_for_integrating_inv(const glm::mat4& T,
                                                      const glm::mat4& R) noexcept {
        auto R_inv = glm::transpose(R);
        auto T_inv = glm::translate(glm::vec3(-T[3]));
        return R_inv * T_inv;
    }

   public:
    Primitive() = delete;

    Primitive(Primitive&&) noexcept = default;
    Primitive& operator=(Primitive&&) noexcept = default;

    Primitive(const Primitive&) = default;
    Primitive& operator=(const Primitive&) = default;

    // clang-format off
    Primitive(
        const glm::mat4& T, const glm::mat4& R, const glm::mat4& S,
        glm::vec3 albedo,
        float optical_depth_scale
    ) : M_for_integrating_inv_(get_M_for_integrating_inv(T, R)),
        S_diag_(getDiagonal(S)),
        S_diag_squared_(glm::pow2(S_diag_)),
        albedo_(albedo),
        optical_depth_scale_(optical_depth_scale) {}

    [[nodiscard]] device::Primitive toDevice() const noexcept override {
        // clang-format off
        return device::Primitive(
            host::toDevice(M_for_integrating_inv_),
            data::toFloat3(S_diag_squared_),
            data::toFloat3(albedo_),
            optical_depth_scale_,
            common::math::ONE_OVER_TWO_ROOT_TWO_F * glm::inversesqrt(glm::compMul(S_diag_))
        );
    }
};

}  // namespace host
}  // namespace thesis
