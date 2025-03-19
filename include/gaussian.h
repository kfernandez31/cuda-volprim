#pragma once

#include "ellipsoid.h"

class Gaussian : public Ellipsoid {
public:
    using Ellipsoid::Ellipsoid;

    Gaussian(const vec3& _color, float _optical_depth_scale, const mat4& _T, const mat4& _R, const mat4& _S)
        : Ellipsoid(_color, _optical_depth_scale, _T, _R, _S) {}

    std::optional<HitRecord> intersect(const Ray& r_global, float t_min) override {
        return intersect_impl(r_global, M_for_intersecting_inv, t_min);
    }

    float density_integral_impl(const Ray& r_global) const {
        auto r_local = r_global.in_coordinate_system(M_for_integrating_inv);
        const auto& x = r_local.origin;
        const auto& w = r_local.direction;

        auto xx = glm::pow2(x);
        auto ww = glm::pow2(w);
        auto xw = x * w;

        auto C_0 = (SS.x * SS.y * ww.z) + (SS.x * SS.z * ww.y) + (SS.y * SS.z * ww.x);
        auto C_0_invsqrt = glm::inversesqrt(C_0);
        auto C_0_sqrt = C_0 * C_0_invsqrt;

        auto C_3 = (xx.x * SS.y + xx.y * SS.x) * ww.z - 2.0f * xw.z * (xw.y * SS.y + xw.x * SS.x);
        auto C_4 = ww.y * (xx.x * SS.z + xx.z * SS.x) - 2.0f * (xw.x * xw.y * SS.z) + ww.x * (xx.y * SS.z + xx.z * SS.y);
        auto C_1 = 0.5f * (C_3 + C_4) * glm::pow2(C_0_invsqrt);

        return glm::exp(-C_1) * C_0_sqrt * glm::one_over_pi<float>();
    }

    float density_integral_impl(const Ray& r_global, const Interval& t_range) const override {
        if (t_range == Interval::universe)
        return density_integral_impl(r_global);

        // TODO: it would be lovely to omit SS completely, i.e. bring the ray into the local coordinate system of an isotropic Gaussian
        auto r_local = r_global.in_coordinate_system(M_for_integrating_inv);
        const auto& x = r_local.origin;
        const auto& w = r_local.direction;

        auto xx = glm::pow2(x);
        auto ww = glm::pow2(w);
        auto xw = x * w;

        auto C_0 = (SS.x * SS.y * ww.z) + (SS.x * SS.z * ww.y) + (SS.y * SS.z * ww.x);
        auto C_0_invsqrt = glm::inversesqrt(C_0);
        auto C_0_sqrt = C_0 * C_0_invsqrt;

        auto C_2 = (x.z * SS.x * SS.y * w.z) + (x.y * SS.x * SS.z * w.y) + (x.x * SS.y * SS.z * w.x);
        auto C_3 = (xx.x * SS.y + xx.y * SS.x) * ww.z - 2.0f * xw.z * (xw.x * SS.x + xw.y * SS.y);
        auto C_4 = ww.y * (xx.x * SS.z + xx.z * SS.x) - 2.0f * (xw.x * xw.y * SS.z) + ww.x * (xx.y * SS.z + xx.z * SS.y);
        auto C_1 = 0.5f * (C_3 + C_4) * glm::pow2(C_0_invsqrt);

        auto denom = C_0_invsqrt * density_integral_erf_denominator_base;
        auto erf_min = std::erf((t_range.min * C_0 + C_2) * denom);
        auto erf_max = std::erf((t_range.max * C_0 + C_2) * denom);

        return glm::exp(-C_1) * C_0_sqrt * (erf_max - erf_min) * glm::one_over_two_pi<float>();
    }
};
