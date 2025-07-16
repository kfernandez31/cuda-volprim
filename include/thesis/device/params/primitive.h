#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/geometry/matrix.h"
#include "thesis/device/geometry/quat.h"

#include <vector_types.h>

#ifdef __CUDACC__
#include "thesis/common/utils/math.h"
#include "thesis/device/geometry/ray.h"

#include <math.h>
#endif  // __CUDACC__

namespace thesis {
namespace device {
namespace params {

class THESIS_ALIGNMENT Primitive {
   private:
    geometry::Matrix3x4 M_for_integrating_inv_;
    float3 S2_;
    float S_det_;
    float S2_xy_, S2_xz_, S2_yz_;
    float erf_denominator_base_;

    geometry::UnitQuaternion rot_quat_;
    float3 scale_;

#ifdef __CUDACC__
    struct OpticalCoefficients {
        float C0, C0_rsqrt, C0_sqrt;
        float C1, C2;
    };

    // ~54 FLOPs, ~60–80 cycles
    __device__ OpticalCoefficients compute_optical_coeffs(const geometry::Ray& ray) const noexcept {
        namespace math = common::math;

        const auto& x = rot_quat_.rotate(M_for_integrating_inv_.transform<true>(ray.origin_));
        const auto& w = rot_quat_.rotate(M_for_integrating_inv_.transform<false>(ray.direction_));

        const auto xx = math::pow2(x);
        const auto ww = math::pow2(w);
        const auto xw = x * w;

        const auto C0 = (S2_xy_ * ww.z) + (S2_xz_ * ww.y) + (S2_yz_ * ww.x);
        const auto C0_rsqrt = rsqrtf(C0);
        const auto C0_sqrt = C0 * C0_rsqrt;

        const auto C2 = (x.z * S2_xy_ * w.z) + (x.y * S2_xz_ * w.y) + (x.x * S2_yz_ * w.x);
        const auto C3 =
            (xx.x * S2_.y + xx.y * S2_.x) * ww.z - 2.0f * xw.z * (xw.y * S2_.y + xw.x * S2_.x);
        const auto C4 = ww.y * (xx.x * S2_.z + xx.z * S2_.x) - 2.0f * (xw.x * xw.y * S2_.z) +
                        ww.x * (xx.y * S2_.z + xx.z * S2_.y);
        const auto C1 = 0.5f * (C3 + C4) * math::pow2(C0_rsqrt);

        return {C0, C0_rsqrt, C0_sqrt, C1, C2};
    }

    __forceinline__ __device__ float optical_depth_internal(const OpticalCoefficients& coeffs,
                                                            float erf_min,
                                                            float erf_max) const noexcept {
        return optical_depth_scale_ * expf(-coeffs.C1) * coeffs.C0_sqrt * (erf_max - erf_min) *
               common::math::ONE_OVER_TWO_PI_F;
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

    // clang-format off
    Primitive(
        const geometry::Matrix3x4& M_for_integrating_inv,
        float3 S_diag_squared,
        float S_det,
        float3 albedo,
        float optical_depth_scale,
        float erf_denominator_base,
        const geometry::UnitQuaternion& rot_quat,
        float3 scale
    )
        : M_for_integrating_inv_(M_for_integrating_inv),
          S2_(S_diag_squared),
          S_det_(S_det),
          S2_xy_(S2_.x * S2_.y),
          S2_xz_(S2_.x * S2_.z),
          S2_yz_(S2_.y * S2_.z),
          erf_denominator_base_(erf_denominator_base),
          albedo_(albedo),
          optical_depth_scale_(optical_depth_scale),
          rot_quat_(rot_quat),
          scale_(scale) {}

#ifdef __CUDACC__
    __forceinline__ __device__ float kernel_pdf(const float3& pos) const noexcept {
        namespace math = common::math;
        const auto local = rot_quat_.rotate(M_for_integrating_inv_.transform<true>(pos));

        const auto pow = -0.5f * math::sum(math::pow2(local) / S2_);
        return expf(pow) * math::ONE_OVER_TWO_PI_POW_3_2_F / S_det_;
    }

    // [t_min, t_max]
    __forceinline__ __device__ float optical_depth(const geometry::Ray& ray_global,
                                              float2 t_range) const noexcept {
        const auto coeffs = compute_optical_coeffs(ray_global);

        const auto denom = coeffs.C0_rsqrt * erf_denominator_base_;
        const auto erf_min = erf((t_range.x * coeffs.C0 + coeffs.C2) * denom);
        const auto erf_max = erf((t_range.y * coeffs.C0 + coeffs.C2) * denom);

        return optical_depth_internal(coeffs, erf_min, erf_max);
    }

    // [t_min, ∞)
    __forceinline__ __device__ float optical_depth(const geometry::Ray& ray_global, float t_min) const noexcept {
        const auto coeffs = compute_optical_coeffs(ray_global);

        const auto denom = coeffs.C0_rsqrt * erf_denominator_base_;
        const auto erf_min = erf((t_min * coeffs.C0 + coeffs.C2) * denom);

        return optical_depth_internal(coeffs, erf_min, 1.0f);
    }

    __forceinline__ __device__ float3 density_integral(const geometry::Ray& ray, float2 t_range) const noexcept {
        return albedo_ * optical_depth(ray, t_range);
    }
    __forceinline__ __device__ float3 density_integral(const geometry::Ray& ray, float t_min) const noexcept {
        return albedo_ * optical_depth(ray, t_min);
    }
#endif  // __CUDACC__
};

}  // namespace params
}  // namespace device
}  // namespace thesis
