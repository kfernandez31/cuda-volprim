#pragma once

#include "ellipsoid.h"

class Gaussian : public Ellipsoid {
public:
    // using Ellipsoid::Ellipsoid;

    Gaussian(const glm::vec3& _color, float _transmittance_scale, const glm::mat4& _T, const glm::mat4& _R, const glm::mat4& _S)
        : Ellipsoid(_color, _transmittance_scale, _T, _R, _S) {}

    // float gauss(float x) const {
    //     return glm::inversesqrt(glm::pow3(2.glm::two_pi<float>() * glm::determinant(transformation_matrix))) * glm::exp(-0.5f * glm::vec4(x - mean, 1.0f) * inv_transformation_matrix * glm::vec4(x - mean, 1.0f));
    // }

    float transmittance(const Ray& r, const Interval& t) const override {
        const auto& x = r.origin;
        auto x_sq = x * x;

        const auto& w = r.direction;
        auto w_sq = w * w;

        const auto& S = M_diagonal;
        auto S_sq = S * S;

        auto C_0 = S_sq[0] * S_sq[1] * w_sq[2]
                 + S_sq[0] * S_sq[2] * w_sq[1]
                 + S_sq[1] * S_sq[2] * w_sq[0];
        auto C_0_invsqrt = glm::inversesqrt(C_0);

        auto C_2 = glm::dot(x * w, S);

        auto C_3 = ((x_sq[0] * S_sq[1]) + (x_sq[1] * S_sq[0])) * w_sq[2]
                 - 2.0f * x[2] * w[2] * ((x[1] * S_sq[0] * w[1]) + (x[0] * S_sq[1] * w[0]));

        auto C_4 = w_sq[1] * (x_sq[0] * S_sq[2] + x_sq[2] * S_sq[0])
                 - 2.0f * x[0] * x[1] * S_sq[2] * w[0] * w[1]
                 + w_sq[0] * (x_sq[1] * S_sq[2] + x_sq[2] * S_sq[1]);

        auto C_1 = 0.5f * (C_3 + C_4) * glm::pow2(C_0_invsqrt);

        auto denominator = C_0_invsqrt * M_det_inv;
        auto erf1 = std::erf((t.min * C_0 + C_2) * denominator);
        auto erf2 = std::erf((t.max * C_0 + C_2) * denominator);

        auto optical_depth = transmittance_scale * glm::exp(-C_1) * C_0 * (erf1 - erf2) * glm::one_over_two_pi<float>() * C_0_invsqrt;
        return glm::exp(-optical_depth);
    }

    // TODO: when to use?
    float transmittance_simplified(const Ray& r, const Interval& t) const {
        const auto& x = r.origin;
        auto x_sq = x * x;

        const auto& w = r.direction;
        auto w_sq = w * w;

        const auto& S = M_diagonal;
        auto S_sq = S * S;

        auto C_0 = S_sq[0] * S_sq[1] * w_sq[2]
                 + S_sq[0] * S_sq[2] * w_sq[1]
                 + S_sq[1] * S_sq[2] * w_sq[0];
        auto C_0_invsqrt = glm::inversesqrt(C_0);

        auto C_3 = ((x_sq[0] * S_sq[1]) + (x_sq[1] * S_sq[0])) * w_sq[2]
                 - 2.0f * x[2] * w[2] * ((x[1] * S_sq[0] * w[1]) + (x[0] * S_sq[1] * w[0]));

        auto C_4 = w_sq[1] * (x_sq[0] * S_sq[2] + x_sq[2] * S_sq[0])
                 - 2.0f * x[0] * x[1] * S_sq[2] * w[0] * w[1]
                 + w_sq[0] * (x_sq[1] * S_sq[2] + x_sq[2] * S_sq[1]);

        auto C_1 = 0.5f * (C_3 + C_4) * glm::pow2(C_0_invsqrt);

        float optical_depth = transmittance_scale * glm::exp(-C_1) * C_0 * glm::one_over_pi<float>() * C_0_invsqrt;
        return glm::exp(-optical_depth);
    }
};
