#pragma once

#include "thesis/device/matrix.h"
#include "thesis/utils/preprocessor.h"

#include <vector_types.h>

#ifdef __CUDACC__
#include "thesis/device/ray.h"
#include "thesis/utils/math.h"
#include <math.h>
#endif  // __CUDACC__

namespace thesis {
namespace device {

class THESIS_ALIGNMENT Primitive {
   private:
    Matrix3x4 M_for_intersecting_ = {};      // TODO(kacper): will I use this?
    Matrix3x4 M_for_intersecting_inv_ = {};  // TODO(kacper): will I use this?
    Matrix3x4 M_for_integrating_inv_ = {};

    float3 S_diag_squared_ = {};
    float3 albedo_ = {};

    float optical_depth_scale_ = 0;
    float erf_denominator_base_ = 0;

// __device__ float Primitive::sigma_t_at(const float3& p) const {
//     // Evaluate unnormalized Gaussian (or other kernel) at point `p`
//     // Use your density kernel scaled by optical_depth_scale_
//     // Example: isotropic Gaussian centered at origin in local space
//     float3 local = transform<true>(M_for_integrating_inv_, p);
//     float3 e2 = -make_float3(
//         local.x * local.x / S_diag_squared_.x,
//         local.y * local.y / S_diag_squared_.y,
//         local.z * local.z / S_diag_squared_.z
//     );
//     return optical_depth_scale_ * expf(e2.x + e2.y + e2.z);
// }

__inline__ __device__ float kernel_pdf(const float3& pos) const noexcept {
    // Transform the point into the local space of the primitive
    const auto local = Matrix3x4::transform<true>(M_for_integrating_inv_, pos);

    // Evaluate the unnormalized density (e.g., Gaussian profile)
    const float3 e2 = -math::pow2(local) / S_diag_squared_;

    return expf(e2.x + e2.y + e2.z);
}

#ifdef __CUDACC__
    __inline__ __device__ float optical_depth(const Ray& r_global) const noexcept {
        const auto r_local = r_global.transformed(M_for_integrating_inv_);
        const auto& x = r_local.origin_;
        const auto& w = r_local.direction_;

        const auto xx = math::pow2(x);
        const auto ww = math::pow2(w);
        const auto xw = x * w;

        const auto C_0 = (S_diag_squared_.x * S_diag_squared_.y * ww.z) +
                         (S_diag_squared_.x * S_diag_squared_.z * ww.y) +
                         (S_diag_squared_.y * S_diag_squared_.z * ww.x);
        const auto C_0_invsqrt = rsqrtf(C_0);
        const auto C_0_sqrt = C_0 * C_0_invsqrt;

        const auto C_3 = (xx.x * S_diag_squared_.y + xx.y * S_diag_squared_.x) * ww.z -
                         2.0f * xw.z * (xw.y * S_diag_squared_.y + xw.x * S_diag_squared_.x);
        const auto C_4 = ww.y * (xx.x * S_diag_squared_.z + xx.z * S_diag_squared_.x) -
                         2.0f * (xw.x * xw.y * S_diag_squared_.z) +
                         ww.x * (xx.y * S_diag_squared_.z + xx.z * S_diag_squared_.y);
        const auto C_1 = 0.5f * (C_3 + C_4) * math::pow2(C_0_invsqrt);

        return optical_depth_scale_ * expf(-C_1) * C_0_sqrt * math::ONE_OVER_PI_F;
    }

    __inline__ __device__ float optical_depth(const Ray& r_global, float2 t_range) const noexcept {
        const auto r_local = r_global.transformed(M_for_integrating_inv_);
        const auto& x = r_local.origin_;
        const auto& w = r_local.direction_;

        const auto xx = math::pow2(x);
        const auto ww = math::pow2(w);
        const auto xw = x * w;

        const auto C_0 = (S_diag_squared_.x * S_diag_squared_.y * ww.z) +
                         (S_diag_squared_.x * S_diag_squared_.z * ww.y) +
                         (S_diag_squared_.y * S_diag_squared_.z * ww.x);
        const auto C_0_invsqrt = rsqrtf(C_0);
        const auto C_0_sqrt = C_0 * C_0_invsqrt;

        const auto C_2 = (x.z * S_diag_squared_.x * S_diag_squared_.y * w.z) +
                         (x.y * S_diag_squared_.x * S_diag_squared_.z * w.y) +
                         (x.x * S_diag_squared_.y * S_diag_squared_.z * w.x);
        const auto C_3 = (xx.x * S_diag_squared_.y + xx.y * S_diag_squared_.x) * ww.z -
                         2.0f * xw.z * (xw.x * S_diag_squared_.x + xw.y * S_diag_squared_.y);
        const auto C_4 = ww.y * (xx.x * S_diag_squared_.z + xx.z * S_diag_squared_.x) -
                         2.0f * (xw.x * xw.y * S_diag_squared_.z) +
                         ww.x * (xx.y * S_diag_squared_.z + xx.z * S_diag_squared_.y);
        const auto C_1 = 0.5f * (C_3 + C_4) * math::pow2(C_0_invsqrt);

        const auto denom = C_0_invsqrt * erf_denominator_base_;
        const auto erf_min = erf((t_range.x * C_0 + C_2) * denom);
        const auto erf_max = erf((t_range.y * C_0 + C_2) * denom);

        return optical_depth_scale_ * expf(-C_1) * C_0_sqrt * (erf_max - erf_min) *
               math::ONE_OVER_TWO_PI_F;
    }
#endif  // __CUDACC__
   public:
    Primitive() = default;

    Primitive(const Matrix3x4& M_for_intersecting, const Matrix3x4& M_for_intersecting_inv,
              const Matrix3x4& M_for_integrating_inv, float3 S_diag_squared, float3 albedo,
              float optical_depth_scale, float erf_denominator_base)
        : M_for_intersecting_(M_for_intersecting),
          M_for_intersecting_inv_(M_for_intersecting_inv),
          M_for_integrating_inv_(M_for_integrating_inv),
          S_diag_squared_(S_diag_squared),
          albedo_(albedo),
          optical_depth_scale_(optical_depth_scale),
          erf_denominator_base_(erf_denominator_base) {}

#ifdef __CUDACC__
    __inline__ __device__ float3 density_integral(const Ray& ray) const noexcept {
        return albedo_ * optical_depth(ray);
    }

    __inline__ __device__ float3 density_integral(const Ray& ray, float2 t_range) const noexcept {
        return albedo_ * optical_depth(ray, t_range);
    }
#endif  // __CUDACC__
};

}  // namespace device
}  // namespace thesis
