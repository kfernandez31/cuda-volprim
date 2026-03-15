#pragma once

#include "thesis/common/geometry/quat.h"
#include "thesis/common/utils/math.h"
#include "thesis/common/utils/preprocessor.h"

#include <vector_types.h>

#ifdef DEVICE
#include "core/constants.cuh"

#include "thesis/device/geometry/ray.h"

#include <math.h>
#endif

namespace thesis {
namespace device {
namespace params {

// Device-side POD struct for primitive (no RAII, same size on host and device)
// Note: rot_quat_ stores the CONJUGATE of the rotation for efficient world-to-local transforms
class THESIS_ALIGNMENT Primitive {
   private:
    float3 center_;
    common::geometry::UnitQuaternion rot_quat_;  // Conjugate for world-to-local
    float3 scale_;
    float3 rcp_scale_;  // 1/scale, precomputed to avoid per-call rcp()

    // Precomputed values for performance
    float density_norm_factor_;
    float inv_cdf_factor_;

   public:
    // Material properties (accessed directly in device code)
    float3 albedo_;
    float optical_thickness_;

    // Default constructor
    Primitive() = default;
    Primitive(const Primitive&) = default;
    Primitive& operator=(const Primitive&) = default;

    // Constructor: precomputes derived values
    // clang-format off
    Primitive(
        float3 center,
        const common::geometry::UnitQuaternion& rot_quat,
        float3 scale,
        float3 albedo,
        float optical_thickness
    )
        : center_(center),
          rot_quat_(rot_quat),
          scale_(scale),
          rcp_scale_(common::math::rcp(scale)),
          density_norm_factor_(common::math::ONE_OVER_TWO_PI_POW_3_2_F * common::math::prod(rcp_scale_)),
          inv_cdf_factor_(optical_thickness * common::math::ONE_OVER_TWO_PI_F * common::math::prod(rcp_scale_)), // TODO: compute in terms of density_norm_factor_. Might require a new constant in math::
          albedo_(albedo),
          optical_thickness_(optical_thickness) {}

    // Getters for introspection
    [[nodiscard]] THESIS_HOST_DEVICE float3 center() const { return center_; }
    // Returns the conjugate quaternion (used for world-to-local transforms)
    [[nodiscard]] THESIS_HOST_DEVICE const common::geometry::UnitQuaternion& rot_quat() const {
        return rot_quat_;
    }
    [[nodiscard]] THESIS_HOST_DEVICE float3 scale() const { return scale_; }

    // Transformation methods (work on both host and device)
    THESIS_HOST_DEVICE THESIS_INLINE float3 transform_pos_local(float3 pos) const {
        return rot_quat_.rotate(pos - center_) * rcp_scale_;
    }

    THESIS_HOST_DEVICE THESIS_INLINE float3 transform_dir_local(float3 dir) const {
        return rot_quat_.rotate(dir) * rcp_scale_;
    }

#ifdef DEVICE
    // Device-only: probability density function
    __device__ __forceinline__ float pdf(float3 pos) const {
        namespace math = common::math;

        const auto local = transform_pos_local(pos);
        const auto len2 = math::length2(local);
        const auto exponent = -0.5f * len2;

        return math::exp(exponent) * density_norm_factor_;
    }

    // Device-only: inverse CDF for importance sampling
    __device__ float inv_cdf(const geometry::Ray& ray, float chi) const {
        namespace math = common::math;

        // Whitened local space
        const auto w = transform_dir_local(ray.direction_);
        const auto p = transform_pos_local(ray.origin_);

        const auto w_len2 = math::length2(w);
        const auto w_inv_len = math::rsqrt(w_len2);

        const auto wp = math::dot(w, p) * w_inv_len;
        const auto pp = math::length2(p);
        const auto diff = math::fma(-wp, wp, pp);  // pp - wp²

        // Compute normalization factor K (matches Mitsuba reference)
        // K = σ_t * (1/|w|) * exp(-0.5*(C-B²)) * (1/2π) * sqrt(prod(scale))
        const auto K = w_inv_len * math::exp(-0.5f * diff) * inv_cdf_factor_;

        auto erfinv_arg = math::erf(wp * math::ONE_OVER_ROOT_TWO_F) + 2.0f * chi * math::rcp(K);

#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
        // Only check for NaN/Inf (actual numerical error)
        // Out-of-range values are valid (ray sampling Gaussian tail) and will be clamped below
        if (!isfinite(erfinv_arg)) {
            printf("ERROR: inv_cdf erfinv_arg is NaN!\n");
            return -1.0f;  // Sentinel: numerical error detected
        }
#endif // THESIS_ENABLE_NUMERICAL_GUARDS

        // Clamp to erfinv domain [-1, 1] (always enabled - handles tail sampling gracefully)
        // When ray samples Gaussian tail, K*chi can be large due to exponential growth (exp(0.5*diff))
        // This is valid physics, not a bug - clamp prevents erfinv from receiving out-of-domain input
        erfinv_arg = math::clamp(erfinv_arg, -1.0f, 1.0f);

        // Result is in whitened arc-length (t * |w|); divide by |w| to get world-space ray parameter t
        return (math::ROOT_TWO_F * erfinv(erfinv_arg) - wp) * w_inv_len;
    }

    // Device-only: optical depth from t0 to infinity
    __device__ float optical_depth(const geometry::Ray& ray, float t0) const {
        namespace math = common::math;

        // Transform to whitened space
        const auto w = transform_dir_local(ray.direction_);
        const auto p = transform_pos_local(ray.origin_);

#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
        // Check for NaN/Inf in transformed values (indicates malformed geometry or corrupted ray)
        if (!isfinite(math::length2(w)) || !isfinite(math::length2(p))) {
            printf("ERROR: optical_depth(t0) NaN/Inf in transforms! w_len2=%f, p_len2=%f\n",
                   math::length2(w), math::length2(p));
            return -1.0f;  // Sentinel: numerical error detected
        }
#endif // THESIS_ENABLE_NUMERICAL_GUARDS

        const auto ray_local = geometry::Ray::spawn_unchecked(p, w);

        // Precompute inverse length
        const auto w_inv_len = math::rlength(w);

#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
        // Check for degenerate direction (extreme scale ratios or zero-length after transform)
        if (!isfinite(w_inv_len) || w_inv_len > 1e10f) {
            printf("ERROR: optical_depth(t0) degenerate direction! w_inv_len=%f\n", w_inv_len);
            return -1.0f;  // Sentinel: degenerate ray
        }
#endif // THESIS_ENABLE_NUMERICAL_GUARDS

        // Point along the ray in local space
        // Clamp t0 to handle numerical imprecision (ray spawning can produce small negative values)
        const auto t0_safe = math::max(0.0f, t0);
        const auto p0 = ray_local.at(t0_safe);

        // Project onto ray direction (normalized)
        const auto w_normalized = w * w_inv_len;
        const auto wp0 = math::dot(w_normalized, p0);

        // Use starting point for the exponential term (integrating to infinity)
        const auto pp0 = math::length2(p0);
        const auto perp_dist2 = math::fma(-wp0, wp0, pp0);  // pp0 - wp0²

        // Common terms
        const auto e_term = math::exp(-0.5f * perp_dist2);
        const auto G_term = math::ROOT_TWO_PI_F * w_inv_len;

        // Complementary error function for integration to infinity
        const auto erf_term = math::erfc(wp0 * math::ROOT_TWO_F);

        return optical_thickness_ * G_term * e_term * erf_term * density_norm_factor_;
    }

    // Device-only: optical depth from t0 to t1
    __device__ float optical_depth(const geometry::Ray& ray, float t0, float t1) const {
        // Handle very small or invalid intervals gracefully
        if (t1 - t0 <= consts::RAY_SEGMENT_MIN_LENGTH) {
            return 0.0f;
        }

        namespace math = common::math;

        // Transform to whitened space
        const auto w = transform_dir_local(ray.direction_);
        const auto p = transform_pos_local(ray.origin_);

#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
        // Check for NaN/Inf in transformed values (indicates malformed geometry or corrupted ray)
        if (!isfinite(math::length2(w)) || !isfinite(math::length2(p))) {
            printf("ERROR: optical_depth(t0,t1) NaN/Inf in transforms! w_len2=%f, p_len2=%f\n",
                   math::length2(w), math::length2(p));
            return -1.0f;  // Sentinel: numerical error detected
        }
#endif // THESIS_ENABLE_NUMERICAL_GUARDS

        const auto ray_local = geometry::Ray::spawn_unchecked(p, w);

        // Precompute inverse length
        const auto w_inv_len = math::rlength(w);

#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
        // Check for degenerate direction (extreme scale ratios or zero-length after transform)
        if (!isfinite(w_inv_len) || w_inv_len > 1e10f) {
            printf("ERROR: optical_depth(t0,t1) degenerate direction! w_inv_len=%f\n", w_inv_len);
            return -1.0f;  // Sentinel: degenerate ray
        }
#endif // THESIS_ENABLE_NUMERICAL_GUARDS

        // Clamp t-values to handle numerical imprecision (ray spawning can produce small negative values)
        const auto t0_safe = math::max(0.0f, t0);
        const auto t1_safe = math::max(t0_safe + consts::RAY_SEGMENT_MIN_LENGTH, t1);

        // Normalize direction
        const auto w_normalized = w * w_inv_len;

        // Shift to starting point using unnormalized direction (t is parameterized by w, not w_hat)
        const auto p_start = ray_local.at(t0_safe);  // = p + t0 * w (unnormalized)
        const auto t_limit = math::clamp((t1_safe - t0_safe) * math::rcp(w_inv_len), 0.0f, math::GAUSSIAN_DIAMETER_F);

        // Projections using START point
        const auto B = math::dot(w_normalized, p_start);
        const auto C = math::length2(p_start);
        const auto perp_dist2 = math::fma(-B, B, C);  // C - B²

        // Common terms
        const auto e_term = math::exp(-0.5f * perp_dist2);
        const auto G_term = math::ROOT_TWO_PI_F * w_inv_len;

        // Error function difference for bounded integration
        // Factor out sqrt(1/2) scaling to reduce operations
        const auto sqrt_half = math::ONE_OVER_ROOT_TWO_F;
        const auto B_scaled = B * sqrt_half;
        const auto erf_plus = math::erf(B_scaled);                 // erf(B/√2)
        const auto erf_minus = math::erf((t_limit - B) * sqrt_half);  // erf((t_limit-B)/√2)
        const auto erf_term_raw = (erf_plus + erf_minus) * 0.5f;

#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
        const auto erf_term = math::clamp(erf_term_raw, -1.0f, 1.0f);
#else
        const auto erf_term = erf_term_raw;
#endif

        return optical_thickness_ * G_term * e_term * erf_term * density_norm_factor_;
    }

    // Device-only: density integral (optical depth * albedo)
    __device__ float3 density_integral(const geometry::Ray& ray, float t0) const {
        auto result = albedo_ * optical_depth(ray, t0);
#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
        // Sanitize: clamp negative values + filter NaN/Inf (defense-in-depth)
        // Catches overflow in optical_depth or negative albedo from bad PLY data
        return common::math::sanitize(result);
#else
        return result;
#endif // THESIS_ENABLE_NUMERICAL_GUARDS
    }

    __device__ float3 density_integral(const geometry::Ray& ray, float t0, float t1) const {
        auto result = albedo_ * optical_depth(ray, t0, t1);
#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
        // Sanitize: clamp negative values + filter NaN/Inf (defense-in-depth)
        // Catches overflow in optical_depth or negative albedo from bad PLY data
        return common::math::sanitize(result);
#else
        return result;
#endif // THESIS_ENABLE_NUMERICAL_GUARDS
    }
#endif  // DEVICE
};

}  // namespace params
}  // namespace device
}  // namespace thesis
