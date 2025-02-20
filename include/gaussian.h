#pragma once

#include "ellipsoid.h"

class Gaussian : public Ellipsoid {
public:
    Gaussian(const vec3& _color, float _optical_depth_scale, const mat4& _T, const mat4& _R, const mat4& _S)
        : Ellipsoid(_color, _optical_depth_scale, _T, _R, _S) {}

    std::optional<HitRecord> intersect(const Ray& r_global) override {
        return intersect_impl(r_global, M_for_intersecting_inv);
    }

    // TODO: when to use?
    /*
    float optical_depth_simplified(const Ray& r_global, const Interval& t) const {
        // Inverting the rotation here isn't necessary but since M_inv is already computed, we don't mind
        auto r_local = r_global.in_coordinate_system(M_inv);
        // r_local.origin = glm::normalize(r_local.origin); // TODO: maybe this is the fix?
        const auto& x = r_local.origin;
        const auto& w = r_local.direction;

        auto xx = glm::pow2(x);
        auto ww = glm::pow2(w);
        auto xw = x * w;

        auto C_0         = glm::compAdd(ww);
        auto C_0_invsqrt = glm::inversesqrt(C_0);

        auto C_3 = (xx.x + xx.y) * ww.z - 2.0f * xw.z * (xw.x + xw.y);
        auto C_4 = ww.y * (xx.x + xx.z) - 2.0f * xw.x * xw.y + ww.x * (xx.y + xx.z);
        auto C_1 = 0.5f * (C_3 + C_4) * glm::pow2(C_0_invsqrt);

        return glm::exp(-C_1) * C_0 * C_0_invsqrt * glm::one_over_pi<float>();
    }
    */

    float optical_depth_impl_tomography(const Ray& r_global) const {
        auto r_local = r_global.in_coordinate_system(M_for_integrating_inv);
        const auto& x = r_local.origin;
        const auto& w = r_local.direction;

        auto xx = glm::pow2(x);
        auto ww = glm::pow2(w);
        auto S_diag = get_diagonal(S);
        auto SS = glm::pow2(S_diag);
        auto xw = x * w;

        auto C_0
            = SS.x * SS.y * ww.z
            + SS.x * SS.z * ww.y
            + SS.y * SS.z * ww.x;
        auto C_0_invsqrt = glm::inversesqrt(C_0);

        auto C_3 = (xx.x * SS.y + xx.y * SS.x) * ww.z - 2.0f * xw.z * (xw.x * SS.x + xw.y * SS.y);
        auto C_4 = ww.y * (xx.x * SS.z + xx.z * SS.x) - 2.0f * xw.x * xw.y * SS.z + ww.x * (xx.y * SS.z + xx.z * SS.y);
        auto C_1 = 0.5f * (C_3 + C_4) * glm::pow2(C_0_invsqrt);

        return glm::exp(-C_1) * C_0 * C_0_invsqrt * glm::one_over_pi<float>();
    }

    float optical_depth_impl(const Ray& r_global, const Interval& t_range) const override {
        auto r_local = r_global.in_coordinate_system(M_for_integrating_inv);
        const auto& x = r_local.origin;
        const auto& w = r_local.direction;

        auto xx = glm::pow2(x);
        auto ww = glm::pow2(w);
        auto S_diag = get_diagonal(S);
        auto SS = glm::pow2(S_diag);
        auto xw = x * w;

        auto C_0
            = SS.x * SS.y * ww.z
            + SS.x * SS.z * ww.y
            + SS.y * SS.z * ww.x;
        auto C_0_invsqrt = glm::inversesqrt(C_0);

        auto C_2
            = x.z * SS.x * SS.y * w.z
            + x.y * SS.x * SS.z * w.y
            + x.x * SS.y * SS.z * w.x;

        auto C_3 = (xx.x * SS.y + xx.y * SS.x) * ww.z - 2.0f * xw.z * (xw.x * SS.x + xw.y * SS.y);
        auto C_4 = ww.y * (xx.x * SS.z + xx.z * SS.x) - 2.0f * xw.x * xw.y * SS.z + ww.x * (xx.y * SS.z + xx.z * SS.y);
        auto C_1 = 0.5f * (C_3 + C_4) * glm::pow2(C_0_invsqrt);

        auto denominator = C_0_invsqrt * glm::inversesqrt(glm::compMul(S_diag)) * glm::one_over_root_two<float>();
        auto erf_min = std::erf((t_range.min * C_0 + C_2) * denominator);
        auto erf_max = std::erf((t_range.max * C_0 + C_2) * denominator);

        return glm::exp(-C_1) * C_0 * C_0_invsqrt * (erf_max - erf_min) * glm::one_over_two_pi<float>();
    }

/*
    float optical_depth(const Ray& r_global, const Interval& t) const override {
        // Inverting the rotation here isn't necessary but since M_inv is already computed, we don't mind
        auto r_local = r_global.in_coordinate_system(M_inv);
        // r_local.origin = glm::normalize(r_local.origin); // TODO: maybe this is the fix?
        const auto& x = r_local.origin;
        const auto& w = r_local.direction;

        auto xx = glm::pow2(x);
        auto ww = glm::pow2(w);
        auto xw   = x * w;

        auto C_0         = glm::compAdd(ww);
        auto C_0_invsqrt = glm::inversesqrt(C_0);
        auto C_2         = glm::dot(x, w);

        auto C_3 = (xx.x + xx.y) * ww.z - 2.0f * xw.z * (xw.x + xw.y);
        auto C_4 = ww.y * (xx.x + xx.z) - 2.0f * xw.x * xw.y + ww.x * (xx.y + xx.z);
        auto C_1 = 0.5f * (C_3 + C_4) * glm::pow2(C_0_invsqrt);

        auto denominator = C_0_invsqrt * glm::one_over_root_two<float>();
        auto erf_min = std::erf((t.min * C_0 + C_2) * denominator);
        auto erf_max = std::erf((t.max * C_0 + C_2) * denominator);

        return glm::exp(-C_1) * C_0 * C_0_invsqrt * (erf1 - erf2) * glm::one_over_two_pi<float>();
    }
*/
};
