#pragma once

#include "thesis/device/geometry/ray.h"
#include "thesis/device/params/primitive.h"
#include "thesis/common/utils/math.h"

namespace thesis {
namespace device {

struct EllipsoidIntersection {
    float t_entry;
    float t_exit;

    EllipsoidIntersection(float t_1, float t_2) t_entry(t_1), t_exit(t_2) {}
    EllipsoidIntersection() t_entry(-1.0f), t_exit(-1.0f) {}

    __device__ bool hit() const { return t_exit > 0.0f; }
    __device__ bool starts_inside() const { return t_entry < 0.0f && t_exit > 0.0f; }
};

// Ray-ellipsoid intersection using numerically stable algorithm
// Transforms ellipsoid to unit sphere in local space, then applies stable ray-sphere intersection
__device__ __forceinline__ EllipsoidIntersection intersect_ellipsoid(
    const geometry::Ray& ray,
    const params::Primitive& prim
) {
    namespace math = common::math;

    // Transform to local whitened space (ellipsoid → unit sphere)
    const auto w = prim.transform_dir_local(ray.direction_);
    const auto p = prim.transform_pos_local(ray.origin_);

    // Numerically stable ray-sphere intersection (adapted from Raytracing Gems, chapter 7)
    const auto a = math::length2(w);

    // Guard against degenerate ray direction
    if (a < 1e-12f) {
        return {};
    }

    const auto a_inv = 1.0f / a;
    const auto b = -math::dot(p, w);
    const auto delta = 1.0f - math::length2(math::fmaf(b * a_inv, w, p)); // delta = 1 - ||p + (b/a)*w||²

    // Guard against miss or tangent (delta ≈ 0 causes q ≈ 0 → division by zero)
    if (delta < 1e-12f) {
       return {};
    }

    const auto c = math::length2(p) - 1.0f;
    const auto q = b + math::copysignf(math::sqrtf(a * delta), b);

    const auto t_1 = c / q;           // Entry
    const auto t_2 = q * a_inv;       // Exit

    // Handle grazing hits (too thin to matter)
    if (t_2 - t_1 <= 1e-8f) {
        return {};
    }

    return {t_1, t_2};
}

// Check if point is inside ellipsoid
__device__ inline bool point_inside_ellipsoid(
    float3 point,
    const params::Primitive& prim
) {
    const auto local = prim.transform_pos_local(point);
    return common::math::length2(local) <= 1.0f;
}

} // namespace device
} // namespace thesis
