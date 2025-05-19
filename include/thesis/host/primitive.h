#pragma once

#include "thesis/device/primitive.h"
#include "thesis/host/convertible.h"
#include "thesis/host/matrix.h"
#include "thesis/utils/data.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/optimum_pow.hpp>
#include <glm/gtx/transform.hpp>

namespace thesis {
namespace host {

class Primitive : public Convertible<device::Primitive> {
    glm::mat4 M_for_intersecting_;
    glm::mat4 M_for_intersecting_inv_;  // TODO(kacper): will I use this?
    glm::mat4 M_for_integrating_inv_;

    glm::vec3 S_diag_;
    glm::vec3 S_diag_squared_;
    glm::vec3 albedo_;

    float optical_depth_scale_;

   private:
    static constexpr auto INTERSECTION_SCALING_FACTOR = 3.0f;

    static inline glm::mat4 get_M_for_intersecting(const glm::mat4& T, const glm::mat4& R,
                                                   const glm::mat4& S) noexcept {
        auto result = T * R * S;
        scaleDiagonal(result, INTERSECTION_SCALING_FACTOR);
        return result;
    }

    static inline glm::mat4 get_M_for_intersecting_inv(const glm::mat4& T, const glm::mat4& R,
                                                       const glm::mat4& S) noexcept {
        auto S_inv = glm::scale(1.0f / (INTERSECTION_SCALING_FACTOR * getDiagonal(S)));
        auto R_inv = glm::transpose(R);
        auto T_inv = glm::translate(glm::vec3(-T[3]));
        return S_inv * R_inv * T_inv;
    }

    static inline glm::mat4 get_M_for_integrating_inv(const glm::mat4& T,
                                                      const glm::mat4& R) noexcept {
        auto R_inv = glm::transpose(R);
        auto T_inv = glm::translate(glm::vec3(-T[3]));
        return R_inv * T_inv;
    }

   public:
    // TODO: a ctor from a normalized quaternion for R
    Primitive(const glm::mat4& T, const glm::mat4& R, const glm::mat4& S, glm::vec3 albedo,
              float optical_depth_scale)
        : M_for_intersecting_(get_M_for_intersecting(T, R, S)),
          M_for_intersecting_inv_(get_M_for_intersecting_inv(T, R, S)),
          M_for_integrating_inv_(get_M_for_integrating_inv(T, R)),
          S_diag_(getDiagonal(S)),
          S_diag_squared_(glm::pow2(S_diag_)),
          albedo_(albedo),
          optical_depth_scale_(optical_depth_scale) {}

    [[nodiscard]] device::Primitive toDevice() const noexcept override {
        return device::Primitive(
            host::toDevice(M_for_intersecting_), host::toDevice(M_for_intersecting_inv_),
            host::toDevice(M_for_integrating_inv_), data::toFloat3(S_diag_squared_),
            data::toFloat3(albedo_), optical_depth_scale_,
            math::ONE_OVER_TWO_SQRT_TWO_F * glm::inversesqrt(glm::compMul(S_diag_)));
    }
};

}  // namespace host
}  // namespace thesis
