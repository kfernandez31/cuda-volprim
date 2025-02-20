#pragma once

#include "interval.h"

class AABB {
private:
    Interval bounds[3];

    // Adjust the AABB so that no side is narrower than some delta, padding if necessary.
    void pad_to_minimums() {
        static auto delta = 0.0001f;
        for (auto& b : bounds)
            if (b.size() < delta)
                b = b.expand(delta);
    }
public:
    const Interval& x() const { return bounds[0]; }
    const Interval& y() const { return bounds[1]; }
    const Interval& z() const { return bounds[2]; }

    AABB() = default;

    AABB(const Interval& x, const Interval& y, const Interval& z)
      : bounds{x, y, z}
    {
        pad_to_minimums();
    }

    AABB(const vec3& a, const vec3& b)
        : AABB(
            {glm::min(a[0], b[0]), glm::max(a[0], b[0])},
            {glm::min(a[1], b[1]), glm::max(a[1], b[1])},
            {glm::min(a[2], b[2]), glm::max(a[2], b[2])}
        ) {}

    AABB(const AABB& diag_1, const AABB& diag_2)
        : AABB(
            {diag_1.x(), diag_2.x()},
            {diag_1.y(), diag_2.y()},
            {diag_1.z(), diag_2.z()}
        ) {}
};