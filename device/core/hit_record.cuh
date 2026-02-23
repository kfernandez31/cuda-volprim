#pragma once

#include <cstdint>

namespace thesis {
namespace device {

// Record of a single ray-primitive entry hit
// Note: With argmin optimization, we only store entry hits (exits computed on-demand)
struct HitRecord {
    float t_hit;      // 4 bytes - distance along ray to entry point
    uint32_t prim_idx;  // 4 bytes - primitive index (no bitfield needed now)

    // Comparison for sorting (unused in argmin, but kept for potential future use)
    __device__ __forceinline__ bool operator<(const HitRecord& other) const {
        if (t_hit < other.t_hit)
            return true;
        if (t_hit > other.t_hit)
            return false;
        return prim_idx < other.prim_idx;
    }
};

}  // namespace device
}  // namespace thesis
