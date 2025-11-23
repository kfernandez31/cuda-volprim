#pragma once

#include "thesis/common/utils/math.h"
#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/geometry/quat.h"

#include <vector_types.h>

#ifdef DEVICE
#include "thesis/device/geometry/ray.h"

#include <math.h>
#endif  // DEVICE

namespace thesis {
namespace device {
namespace params {

class THESIS_ALIGNMENT Primitive {
   private:
    float3 center_;
    geometry::UnitQuaternion rot_quat_;
    float3 scale_;

    float scale_det_;
    float one_over_scale_det_;
    float density_norm_factor_;
    float inv_cdf_factor_;

#ifdef DEVICE
    __device__ float3 transform_pos_local(float3 pos) const {
        return rot_quat_.rotate(pos - center_) / scale_;
    }

    __device__ float3 transform_dir_local(float3 dir) const {
        return rot_quat_.rotate(dir) / scale_;
    }

#endif  // DEVICE

   public:
    float3 albedo_;
    float optical_thickness_;

    Primitive() = default;
    Primitive(const Primitive&) = default;
    Primitive& operator=(const Primitive&) = default;

    // clang-format off
    Primitive(
        float3 center,
        const geometry::UnitQuaternion& rot_quat,
        float3 scale,
        float3 albedo,
        float optical_thickness
    )
        : center_(center),
          rot_quat_(rot_quat),
          scale_(scale),
          scale_det_(common::math::prod(scale)),
          one_over_scale_det_(1 / scale_det_),
          density_norm_factor_(common::math::ONE_OVER_TWO_PI_POW_3_2_F * one_over_scale_det_),
          inv_cdf_factor_(common::math::FOUR_PI_F * scale_det_ / optical_thickness),
          albedo_(albedo),
          optical_thickness_(optical_thickness) {}

#ifdef DEVICE
    __device__ float pdf(float3 pos) const {
        namespace math = common::math;

        const auto local = transform_pos_local(pos);
        const auto len2 = math::length2(local);
        const auto exponent = -0.5f * len2;

        return __expf(exponent) * density_norm_factor_;
    }

    __device__ float inv_cdf(const geometry::Ray& ray, float chi) const {
        namespace math = common::math;

        // Whitened local space
        const auto w = transform_dir_local(ray.direction_);
        const auto p = transform_pos_local(ray.origin_);

        const auto w_len2 = math::length2(w);
        const auto w_inv_len = rsqrtf(w_len2);
        const auto w_len = w_len2 * w_inv_len;

        const auto wp = math::dot(w, p) * w_inv_len;
        const auto pp = math::dot(p, p);
        const auto diff = __fmaf_rn(-wp, wp, pp); // pp - wp²
        const auto exponent = 0.5f * diff;

        // Compute normalization factor K
        const auto K = w_len * __expf(exponent) * inv_cdf_factor_;

        // Clamp to avoid NaNs
        auto erfinv_arg = erf(wp * math::ROOT_TWO_F) + chi * K;
        erfinv_arg = math::clamp(erfinv_arg, -1.0f, 1.0f);  // clamp to avoid NaN

        return math::ROOT_TWO_F * erfinv(erfinv_arg) - wp;
    }


    __device__ float optical_depth(const geometry::Ray& ray, float t0) const {
        namespace math = common::math;

        // Transform to whitened space
        const auto w = transform_dir_local(ray.direction_);
        const auto p = transform_pos_local(ray.origin_);
        const auto ray_local = geometry::Ray::spawn_unchecked(p, w);

        // Precompute length and inverse
        const auto w_len2 = math::length2(w);
        const auto w_inv_len = rsqrtf(w_len2);

        // Point along the ray in local space
        const auto p0 = ray_local.at(t0);

        // Project onto ray direction (normalized)
        const auto w_normalized = w * w_inv_len;
        const auto wp0 = math::dot(w_normalized, p0);

        // Use starting point for the exponential term (integrating to infinity)
        const auto pp0 = math::dot(p0, p0);
        const auto perp_dist2 = __fmaf_rn(-wp0, wp0, pp0);  // FMA: pp0 + (-wp0)*wp0

        // Common terms
        const auto e_term = __expf(-0.5f * perp_dist2);
        const auto G_term = math::ROOT_TWO_PI_F * w_inv_len;

        // Complementary error function for integration to infinity
        const auto erf_term = erfcf(wp0 * math::ROOT_TWO_F);

        return optical_thickness_ * G_term * e_term * erf_term;
    }

    __device__ float optical_depth(const geometry::Ray& ray, float t0, float t1) const {
        assert(t0 <= t1 && isfinite(t1) && t1 > 0.0f);
        // Handle very small intervals (numerical precision)
        if (t1 - t0 <= 1e-6f) return 0.0f;

        namespace math = common::math;

        // Transform to whitened space
        const auto w = transform_dir_local(ray.direction_);
        const auto p = transform_pos_local(ray.origin_);
        const auto ray_local = geometry::Ray::spawn_unchecked(p, w);

        // Precompute length and inverse
        const auto w_len2 = math::length2(w);
        const auto w_inv_len = rsqrtf(w_len2);

        // Points along the ray in local space
        const auto p0 = ray_local.at(t0);
        const auto p1 = ray_local.at(t1);

        // Project onto ray direction (normalized)
        const auto w_normalized = w * w_inv_len;
        const auto wp0 = math::dot(w_normalized, p0);
        const auto wp1 = math::dot(w_normalized, p1);

        // Use the midpoint for the exponential term
        const auto mid_p = 0.5f * (p0 + p1);
        const auto wp_mid = 0.5f * (wp0 + wp1);
        const auto pp_mid = math::dot(mid_p, mid_p);
        const auto perp_dist2 = __fmaf_rn(-wp_mid, wp_mid, pp_mid);  // FMA: pp_mid + (-wp_mid)*wp_mid

        // Common terms
        const auto e_term = __expf(-0.5f * perp_dist2);
        const auto G_term = math::ROOT_TWO_PI_F * w_inv_len;

        // Error function difference for bounded integration
        const auto erf_term = erff(wp1 * math::ROOT_TWO_F) - erff(wp0 * math::ROOT_TWO_F);

        return optical_thickness_ * G_term * e_term * erf_term;
    }

    __device__ float3 density_integral(const geometry::Ray& ray, float t0) const {
        auto result = albedo_ * optical_depth(ray, t0);
#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
        return common::math::sanitize(result);
#else
        return result;
#endif
    }

    __device__ float3 density_integral(const geometry::Ray& ray, float t0, float t1) const {
        auto result = albedo_ * optical_depth(ray, t0, t1);
#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
        return common::math::sanitize(result);
#else
        return result;
#endif
    }
#endif  // DEVICE
};

}  // namespace params
}  // namespace device
}  // namespace thesis
