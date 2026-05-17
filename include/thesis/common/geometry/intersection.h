#pragma once

#include "thesis/common/utils/math.h"
#include "thesis/common/utils/preprocessor.h"

#include <vector_types.h>

namespace thesis {
namespace device {
namespace params {
class Primitive;
}  // namespace params
}  // namespace device
}  // namespace thesis

#ifdef __CUDA_ARCH__
#include "core/constants.cuh"

#include "thesis/device/geometry/ray.h"
#endif  // DEVICE

namespace thesis {
namespace common {
namespace geometry {

struct EllipsoidIntersection {
    float t_entry;
    float t_exit;
    float w_len2;  // Cached ||w||² for exit recomputation (avoids redundant transform)

    THESIS_HOST_DEVICE THESIS_INLINE EllipsoidIntersection(float t_1, float t_2,
                                                           float w_len2_val = 0.0f)
        : t_entry(t_1),
          t_exit(t_2),
          w_len2(w_len2_val) {}
    THESIS_HOST_DEVICE THESIS_INLINE EllipsoidIntersection()
        : t_entry(-1.0f),
          t_exit(-1.0f),
          w_len2(0.0f) {}

    THESIS_HOST_DEVICE THESIS_INLINE bool is_hit() const { return t_exit > 0.0f; }
    THESIS_HOST_DEVICE THESIS_INLINE bool starts_inside() const {
        return t_entry < 0.0f && t_exit > 0.0f;
    }
};

// Check if a world-space point lies inside the primitive's BVH bound (3σ ellipsoid).
//
// Matches what OptiX actually traverses: localToWorld() inflates scale by
// GAUSSIAN_EXTENT_F (= 3), so the BVH wraps the 3σ surface. A 1σ test here
// would miss primitives whose 3σ shell contains the origin but whose 1σ core
// does not — and since OptiX 9's built-in sphere intersector only reports
// front-face entries (no hit when origin is inside the sphere), such primitives
// would also be absent from hit_buffer. Result: they would contribute zero to
// optical depth and scatter sampling — the dominant systematic-review bug.
THESIS_HOST_DEVICE THESIS_INLINE bool point_inside_bvh_bound(
    float3 point, const device::params::Primitive& prim) {
    const auto local = prim.transform_pos_local(point);
    constexpr float R2 = math::GAUSSIAN_EXTENT_F * math::GAUSSIAN_EXTENT_F;  // 3σ surface radius²
    return math::length2(local) <= R2;
}

#ifdef __CUDA_ARCH__
// Ray-ellipsoid intersection using numerically stable algorithm
// Transforms ellipsoid to unit sphere in local space, then applies stable ray-sphere intersection
__device__ __forceinline__ EllipsoidIntersection
intersect_ellipsoid(const device::geometry::Ray& ray, const device::params::Primitive& prim) {
    // Transform to local whitened space (ellipsoid → unit sphere)
    const auto w = prim.transform_dir_local(ray.direction_);
    const auto p = prim.transform_pos_local(ray.origin_);

    // Numerically stable ray-sphere intersection (adapted from Raytracing Gems, chapter 7)
    const auto a = math::length2(w);

    // Guard against degenerate ray direction (near-zero length)
    if (a < device::consts::RAY_DIRECTION_MIN_LENGTH2) {
        return {};
    }

    const auto a_inv = math::rcp(a);
    const auto b = -math::dot(p, w);
    const auto delta =
        1.0f - math::length2(math::fma(b * a_inv, w, p));  // delta = 1 - ||p + (b/a)*w||²

    // Guard against miss or tangent hit (delta ≈ 0 causes q ≈ 0 → division by zero)
    if (delta < device::consts::INTERSECTION_DISCRIMINANT_EPS) {
        return {};
    }

    const auto c = math::length2(p) - 1.0f;
    const auto q = b + math::copysign(math::sqrt(a * delta), b);

    const auto t_1 = c * math::rcp(q);  // Entry
    const auto t_2 = q * a_inv;         // Exit

    // Note: We intentionally allow thin segments through here.
    // The optical depth integration handles thin segments correctly (they contribute ~0),
    // and rejecting them here would cause mismatch with OptiX hardware intersection.

    return {t_1, t_2, a};  // Cache a (w_len2) for potential exit recomputation
}

// Exit t for a ray whose ORIGIN lies inside the 3σ BVH bound (full quadratic).
//
// Used by sample_scattering_event / compute_escape_optical_depth /
// compute_transmittance_to_env when iterating active_prims — those are the
// primitives the ray origin is already inside, so OptiX reports no entry and
// `compute_exit_from_entry(ray, 0.0f, ...)` would be wrong (its derivation
// assumes the entry lies on the surface, |p|² = R²).
//
// Solving |p + t·w|² = R² with R² = GAUSSIAN_EXTENT_F² and origin inside:
//   t²|w|² + 2t(p·w) + (|p|² - R²) = 0
// |p|² < R² so the constant term is negative → discriminant strictly positive.
// The positive root is the exit ahead of the ray.
__device__ __forceinline__ float exit_from_inside(const device::geometry::Ray& ray,
                                                  const device::params::Primitive& prim) {
    const auto w = prim.transform_dir_local(ray.direction_);
    const auto p = prim.transform_pos_local(ray.origin_);
    const auto a = math::length2(w);
    const auto b = math::dot(p, w);
    constexpr float R2 = math::GAUSSIAN_EXTENT_F * math::GAUSSIAN_EXTENT_F;
    const auto c = math::length2(p) - R2;
    // Origin inside ⇒ c < 0 ⇒ disc = b² - a·c > 0; positive root is the exit.
    const auto disc = math::fma(-a, c, b * b);
    return (-b + math::sqrt(disc)) * math::rcp(a);
}

// Optimized exit computation from entry point (avoids full intersection solve)
// Given entry t-value on ellipsoid surface, analytically computes exit t-value
//
// Derivation: For point p on the 3σ surface (|p|² = R² with R = GAUSSIAN_EXTENT_F),
//             solving |p + t·w|² = R² gives t = 0 (entry) or t = -2(p·w)/|w|² (exit)
//
// Key insight: The parameter t is invariant under affine transformation, so the
//              exit distance computed in local space equals the exit distance in
//              world space (no scaling needed).
//
// This eliminates: discriminant checks, epsilon offsets, and full quadratic solve.
//
// Precondition: t_entry must put the ray at a point ON the surface (|p_t_entry|² = R²).
// Use `exit_from_inside` instead for ray origins already inside the BVH bound.
__device__ __forceinline__ float compute_exit_from_entry(const device::geometry::Ray& ray,
                                                         float t_entry,
                                                         const device::params::Primitive& prim,
                                                         float w_len2) {
    // Transform entry point and ray direction to local whitened space
    const auto entry_point = ray.at(t_entry);
    const auto p = prim.transform_pos_local(entry_point);
    const auto w = prim.transform_dir_local(ray.direction_);

#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
    // Guard against degenerate direction (compile-time optional)
    if (w_len2 < device::consts::RAY_DIRECTION_MIN_LENGTH2) {
        return -1.0f;
    }
#endif  // THESIS_ENABLE_NUMERICAL_GUARDS

    const auto p_dot_w = math::dot(p, w);

    // Analytical formula: t_exit = -2(p·w) / |w|²
    // This is exact when |p|² = 1 and numerically stable for |p|² ≈ 1
    const auto exit_dist = -2.0f * p_dot_w * math::rcp(w_len2);

#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
    // Sanity check that exit distance is positive (compile-time optional)
    if (exit_dist <= 0.0f) {
        return -1.0f;
    }
#endif  // THESIS_ENABLE_NUMERICAL_GUARDS

    // Parametric distance is invariant under transformation
    return t_entry + exit_dist;
}
#endif  // DEVICE

}  // namespace geometry
}  // namespace common
}  // namespace thesis
