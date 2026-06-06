#pragma once

#include "thesis/common/geometry/quat.h"
#include "thesis/common/utils/math.h"
#include "thesis/common/utils/preprocessor.h"

#include <vector_types.h>

#ifdef __CUDA_ARCH__
#include "core/constants.cuh"

#include "thesis/device/geometry/ray.h"

#include <math.h>
#else
#include "thesis/host/utils/math.h"
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
          inv_cdf_factor_(optical_thickness * density_norm_factor_ * common::math::ROOT_HALF_PI_F),
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

#ifndef __CUDA_ARCH__
    // Host-only: construct from forward quaternion (conjugates internally for world-to-local)
    static Primitive from_forward_quat(float3 center, const common::geometry::UnitQuaternion& forward_quat,
                                       float3 scale, float3 albedo, float optical_thickness) {
        return Primitive(center, forward_quat.conjugate(), scale, albedo, optical_thickness);
    }

    // Host-only: derive forward rotation from stored conjugate
    [[nodiscard]] common::geometry::UnitQuaternion forward_rot_quat() const {
        return rot_quat_.conjugate();
    }

    // Host-only: generate OptiX transformation matrix (local-to-world)
    [[nodiscard]] host::utils::math::Mat3x4 localToWorld() const noexcept {
        const auto scaled = scale_ * common::math::GAUSSIAN_EXTENT_F;
        return host::utils::math::Mat3x4::from_trs(center_, forward_rot_quat(), scaled);
    }
#endif  // !DEVICE

#ifdef __CUDA_ARCH__
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
        // K = σ_t × density_norm_factor × √(2π)/2 × (1/|w|) × exp(-0.5×perp²)
        // The factor of 2 from the erfinv equation is absorbed into inv_cdf_factor_
        const auto K = w_inv_len * math::exp(-0.5f * diff) * inv_cdf_factor_;

        const auto raw_arg = math::erf(wp * math::ONE_OVER_ROOT_TWO_F) + chi * math::rcp(K);

#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
        // Only check for NaN/Inf (actual numerical error)
        if (!isfinite(raw_arg)) {
            printf("ERROR: inv_cdf erfinv_arg is NaN!\n");
            return -1.0f;  // Sentinel: numerical error detected
        }
#endif // THESIS_ENABLE_NUMERICAL_GUARDS

        // Saturated above 1.0 ⇒ the primitive's remaining mass along the ray is less
        // than the requested optical-depth threshold (chi). The primitive cannot
        // produce a scatter on this sample — return +∞ so the ADT-min in the caller
        // rejects it cleanly. Matches papers/jorge_python.py:43-45 which lets erfinv
        // produce NaN and converts NaN → dr.inf for the same case.
        if (raw_arg > 1.0f) return consts::INF_F;
        const auto erfinv_arg = math::clamp(raw_arg, -1.0f, 1.0f);

        // Result is in whitened arc-length (t * |w|); divide by |w| to get world-space ray parameter t
        return (math::ROOT_TWO_F * erfinv(erfinv_arg) - wp) * w_inv_len;
    }

    // Device-only: free-flight inverse CDF restricted to t ≥ t_offset.
    // Solves optical_depth(ray, t_offset, t_scatter) = chi for t_scatter.
    //
    // Used by ADT scatter sampling for primitives the ray enters mid-flight
    // (hit_buffer case in sample_scattering_event). Sampling from the full
    // Gaussian CDF and rejecting t < t_offset is biased: rejected samples are
    // dropped rather than re-rolled, so the primitive systematically
    // under-contributes scatter events. This variant samples directly from the
    // segment-restricted CDF, yielding unbiased free-flight per SDTracking §4.1.
    //
    // Math: identical to inv_cdf except the erf reference shifts from
    //   erf(wp/√2)  →  erf((wp + t_offset·|w|)/√2)
    // The perpendicular distance in whitened space is invariant under shifts
    // along the ray, so K (and hence the per-step erf advance chi/K) is unchanged.
    __device__ float inv_cdf_segment(const geometry::Ray& ray, float t_offset, float chi) const {
        namespace math = common::math;

        const auto w = transform_dir_local(ray.direction_);
        const auto p = transform_pos_local(ray.origin_);

        const auto w_len2 = math::length2(w);
        const auto w_inv_len = math::rsqrt(w_len2);
        const auto w_len = w_len2 * w_inv_len;  // |w| reusing rsqrt result

        const auto wp = math::dot(w, p) * w_inv_len;
        const auto pp = math::length2(p);
        const auto diff = math::fma(-wp, wp, pp);  // pp - wp²

        const auto K = w_inv_len * math::exp(-0.5f * diff) * inv_cdf_factor_;

        // Shift the erf reference from t=0 to t=t_offset.
        const auto wp_off = math::fma(t_offset, w_len, wp);
        const auto raw_arg = math::erf(wp_off * math::ONE_OVER_ROOT_TWO_F) + chi * math::rcp(K);

#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
        if (!isfinite(raw_arg)) {
            printf("ERROR: inv_cdf_segment erfinv_arg is NaN!\n");
            return -1.0f;
        }
#endif // THESIS_ENABLE_NUMERICAL_GUARDS

        // Saturated above 1.0 ⇒ the remaining segment mass < chi (the optical-depth
        // threshold). No scatter possible from this primitive on this sample — return
        // +∞ for an explicit ADT-min rejection. Matches papers/jorge_python.py:43-45
        // (NaN → dr.inf). Previously this relied on erfinv(1.0f) ≈ FLT_MAX combined
        // with the downstream `t_scatter <= t_exit` check, which worked by accident.
        if (raw_arg > 1.0f) return consts::INF_F;
        const auto erfinv_arg = math::clamp(raw_arg, -1.0f, 1.0f);

        return (math::ROOT_TWO_F * erfinv(erfinv_arg) - wp) * w_inv_len;
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

        // Error function difference for bounded integration.
        // Factor out sqrt(1/2) scaling and reuse B_scaled via FMA to avoid recomputing B*sqrt_half.
        const auto sqrt_half = math::ONE_OVER_ROOT_TWO_F;
        const auto B_scaled = B * sqrt_half;
        const auto erf_lower = math::fast_erf(B_scaled);                              // erf(B/√2)
        const auto erf_upper = math::fast_erf(math::fma(t_limit, sqrt_half, B_scaled));  // erf((B+t_limit)/√2)
        const auto erf_term_raw = (erf_upper - erf_lower) * 0.5f;

#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
        const auto erf_term = math::clamp(erf_term_raw, -1.0f, 1.0f);
#else
        const auto erf_term = erf_term_raw;
#endif

        return optical_thickness_ * G_term * e_term * erf_term * density_norm_factor_;
    }

    // Device-only: density integral (optical depth * albedo)
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
