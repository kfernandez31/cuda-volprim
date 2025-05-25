#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/geometry/matrix.h"

#include <vector_types.h>

#ifdef __CUDACC__
#include "thesis/common/utils/math.h"
#include "thesis/device/geometry/ray.h"

#include <math.h>
#endif  // __CUDACC__

namespace thesis {
namespace device {

class THESIS_ALIGNMENT Primitive {
   private:
    Matrix3x4 M_for_intersecting_;      // TODO(kacper): will I use this?
    Matrix3x4 M_for_intersecting_inv_;  // TODO(kacper): will I use this?
    Matrix3x4 M_for_integrating_inv_;

    float3 S_diag_squared_;
    float erf_denominator_base_;

#ifdef __CUDACC__
    struct OpticalCoefficients {
        float C0, C0_rsqrt, C0_sqrt;
        float C1, C2;
    };

    __device__ OpticalCoefficients compute_optical_coeffs(const Ray& r_global) const noexcept {
        const auto& S2 = S_diag_squared_;

        const auto r_local = r_global.transformed(M_for_integrating_inv_);
        const auto& x = r_local.origin_;
        const auto& w = r_local.direction_;

        const auto xx = math::pow2(x);
        const auto ww = math::pow2(w);
        const auto xw = x * w;

        const auto C0 = (S2.x * S2.y * ww.z) + (S2.x * S2.z * ww.y) + (S2.y * S2.z * ww.x);
        const auto C0_rsqrt = rsqrtf(C0);
        const auto C0_sqrt = C0 * C0_rsqrt;

        const auto C2 =
            (x.z * S2.x * S2.y * w.z) + (x.y * S2.x * S2.z * w.y) + (x.x * S2.y * S2.z * w.x);
        const auto C3 =
            (xx.x * S2.y + xx.y * S2.x) * ww.z - 2.0f * xw.z * (xw.y * S2.y + xw.x * S2.x);
        const auto C4 = ww.y * (xx.x * S2.z + xx.z * S2.x) - 2.0f * (xw.x * xw.y * S2.z) +
                        ww.x * (xx.y * S2.z + xx.z * S2.y);
        const auto C1 = 0.5f * (C3 + C4) * math::pow2(C0_rsqrt);

        return {C0, C0_rsqrt, C0_sqrt, C1, C2};
    }

    __inline__ __device__ float optical_depth_internal(const OpticalCoefficients& coeffs,
                                                       float erf_min,
                                                       float erf_max) const noexcept {
        return optical_depth_scale_ * expf(-coeffs.C1) * coeffs.C0_sqrt * (erf_max - erf_min) *
               math::ONE_OVER_TWO_PI_F;
    }
#endif  // __CUDACC__

   public:
    float3 albedo_;
    float optical_depth_scale_;

    Primitive() = default;

    Primitive(Primitive&&) noexcept = default;
    Primitive& operator=(Primitive&&) noexcept = default;

    Primitive(const Primitive&) = default;
    Primitive& operator=(const Primitive&) = default;

    Primitive(const Matrix3x4& M_for_intersecting, const Matrix3x4& M_for_intersecting_inv,
              const Matrix3x4& M_for_integrating_inv, float3 S_diag_squared, float3 albedo,
              float optical_depth_scale, float erf_denominator_base)
        : M_for_intersecting_(M_for_intersecting),
          M_for_intersecting_inv_(M_for_intersecting_inv),
          M_for_integrating_inv_(M_for_integrating_inv),
          S_diag_squared_(S_diag_squared),
          erf_denominator_base_(erf_denominator_base),
          albedo_(albedo),
          optical_depth_scale_(optical_depth_scale) {}

#ifdef __CUDACC__
    // TODO(kacper): validate w/Jorge
    __inline__ __device__ float kernel_pdf(const float3& pos) const noexcept {
        // Transform the point into the local space of the primitive
        const auto local = Matrix3x4::transform<true>(M_for_integrating_inv_, pos);

        // Evaluate the unnormalized density (e.g., Gaussian profile)
        const auto e2 = -math::pow2(local) / S_diag_squared_;

        return expf(math::sum(e2));
    }

    // (-∞, ∞)
    __inline__ __device__ float optical_depth(const Ray& ray_global) const noexcept {
        const auto coeffs = compute_optical_coeffs(ray_global);

        return optical_depth_scale_ * expf(-coeffs.C1) * coeffs.C0_sqrt * math::ONE_OVER_PI_F;
    }

    // [t_min, t_max]
    __inline__ __device__ float optical_depth(const Ray& ray_global,
                                              float2 t_range) const noexcept {
        const auto coeffs = compute_optical_coeffs(ray_global);

        const auto denom = coeffs.C0_rsqrt * erf_denominator_base_;
        const auto erf_min = erf((t_range.x * coeffs.C0 + coeffs.C2) * denom);
        const auto erf_max = erf((t_range.y * coeffs.C0 + coeffs.C2) * denom);

        return optical_depth_internal(coeffs, erf_min, erf_max);
    }

    // [t_min, ∞)
    __inline__ __device__ float optical_depth(const Ray& ray_global, float t_min) const noexcept {
        const auto coeffs = compute_optical_coeffs(ray_global);

        const auto denom = coeffs.C0_rsqrt * erf_denominator_base_;
        const auto erf_min = erf((t_min * coeffs.C0 + coeffs.C2) * denom);

        return optical_depth_internal(coeffs, erf_min, 1.0f);
    }

    __inline__ __device__ float3 density_integral(const Ray& ray) const noexcept {
        return albedo_ * optical_depth(ray);
    }
    __inline__ __device__ float3 density_integral(const Ray& ray, float2 t_range) const noexcept {
        return albedo_ * optical_depth(ray, t_range);
    }
    __inline__ __device__ float3 density_integral(const Ray& ray, float t_min) const noexcept {
        return albedo_ * optical_depth(ray, t_min);
    }
#endif  // __CUDACC__
};

}  // namespace device
}  // namespace thesis
