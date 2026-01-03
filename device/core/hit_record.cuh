#pragma once

#include <cstdint>

namespace thesis {
namespace device {

// Record of a single ray-primitive intersection
struct HitRecord {
    float t_hit;
    uint32_t prim_idx : 31;
    uint32_t is_exit : 1;

    // Comparison: primary key is t_hit, with deterministic tie-breaking
    __device__ __forceinline__ bool operator<(const HitRecord& other) const {
        if (t_hit < other.t_hit) return true;
        if (t_hit > other.t_hit) return false;
        return prim_idx < other.prim_idx;
    }
};

} // namespace device
} // namespace thesis
