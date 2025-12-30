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

#ifdef DEVICE
#include "core/constants.cuh"

#include "thesis/device/geometry/ray.h"
#endif  // DEVICE

namespace thesis {
namespace common {
namespace geometry {

struct EllipsoidIntersection {
    float t_entry;
    float t_exit;

    THESIS_HOST_DEVICE THESIS_INLINE EllipsoidIntersection(float t_1, float t_2)
        : t_entry(t_1),
          t_exit(t_2) {}
    THESIS_HOST_DEVICE THESIS_INLINE EllipsoidIntersection()
        : t_entry(-1.0f),
          t_exit(-1.0f) {}

    THESIS_HOST_DEVICE THESIS_INLINE bool is_hit() const { return t_exit > 0.0f; }
    THESIS_HOST_DEVICE THESIS_INLINE bool starts_inside() const {
        return t_entry < 0.0f && t_exit > 0.0f;
    }
};

// Check if point is inside ellipsoid
THESIS_HOST_DEVICE THESIS_INLINE bool point_inside_ellipsoid(
    float3 point, const device::params::Primitive& prim) {
    const auto local = prim.transform_pos_local(point);
    return math::length2(local) <= 1.0f;
}

#ifdef DEVICE
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

    // Handle grazing hits (intersection too thin to contribute meaningfully)
    if (t_2 - t_1 <= device::consts::INTERSECTION_MIN_SEGMENT_LENGTH) {
        return {};
    }

    return {t_1, t_2};
}
#endif  // DEVICE

}  // namespace geometry
}  // namespace common
}  // namespace thesis
