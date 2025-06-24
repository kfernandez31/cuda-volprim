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

namespace thesis::host::params {

class Primitive : public Convertible<device::params::Primitive> {
   private:
    glm::mat4 M_for_intersecting_, M_for_integrating_inv_;
    glm::vec3 S_diag_, S_diag_squared_;
    glm::vec3 albedo_;
    float optical_depth_scale_;

    static constexpr auto INTERSECTION_SCALING_FACTOR = 3.0f;

    static inline glm::mat4 get_M_for_intersecting(const glm::mat4& T, const glm::mat4& R,
                                                   const glm::mat4& S) noexcept {
        const auto scale = glm::scale(glm::vec3(INTERSECTION_SCALING_FACTOR));
        return T * R * (S * scale);
    }

    static inline glm::mat4 get_M_for_integrating_inv(const glm::mat4& T, const glm::mat4& R,
                                                      const glm::mat4& S) noexcept {
        const auto R_inv = glm::transpose(R);
        // const auto S_inv = glm::scale(1.0f / geometry::getDiagonal(S)); // TODO(kacper): remove?
        const auto S_inv = glm::identity<glm::mat4>();
        const auto T_inv = glm::translate(-glm::vec3(T[3]));
        return S_inv * R_inv * T_inv;
    }

   public:
    Primitive() = delete;

    Primitive(Primitive&&) noexcept = default;
    Primitive& operator=(Primitive&&) noexcept = default;

    Primitive(const Primitive&) = default;
    Primitive& operator=(const Primitive&) = default;

    const glm::mat4& M() const noexcept { return M_for_intersecting_; }

    // clang-format off
    Primitive(
        const glm::mat4& T, const glm::mat4& R, const glm::mat4& S,
        glm::vec3 albedo,
        float optical_depth_scale
    ) : M_for_intersecting_(get_M_for_intersecting(T, R, S)),
        M_for_integrating_inv_(get_M_for_integrating_inv(T, R, S)),
        S_diag_(geometry::getDiagonal(S)),
        S_diag_squared_(glm::pow2(S_diag_)),
        albedo_(albedo),
        optical_depth_scale_(optical_depth_scale) {}

    [[nodiscard]] device::params::Primitive toDevice() const noexcept override {
        // clang-format off
        return device::params::Primitive(
            host::geometry::toDevice(M_for_integrating_inv_),
            utils::data::toFloat3(S_diag_squared_),
            utils::data::toFloat3(albedo_),
            optical_depth_scale_,
            common::math::ONE_OVER_TWO_ROOT_TWO_F * glm::inversesqrt(glm::compMul(S_diag_))
        );
    }
};

}  // namespace thesis::host::params
