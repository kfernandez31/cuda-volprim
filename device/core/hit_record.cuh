#pragma once

#include "thesis/common/utils/types.h"

#include <cstdint>

namespace thesis {
namespace device {

// Unified structure for ray-primitive intersections (entries and exits)
// Used for:
// 1. Hit collection during ray tracing (is_exit = false, from anyhit shader)
// 2. Event processing in escape case (is_exit distinguishes entries/exits)
// 3. Cached exits in argmin loop (is_exit = true)
//
// Memory layout: float (4B) + prim_idx_t (2B) + bitfield (within prim_idx_t word) = 8 bytes
struct HitRecord {
    float t_hit;            // 4 bytes - distance along ray
    prim_idx_t prim_idx;    // 2 bytes - primitive index
    prim_idx_t is_exit : 1; // 1 bit   - false for entry, true for exit

    // Comparison for sorting by t-value (with deterministic tie-breaking)
    __device__ __forceinline__ bool operator<(const HitRecord& other) const {
        if (t_hit < other.t_hit)
            return true;
        if (t_hit > other.t_hit)
            return false;
        // Tie-breaker: entries before exits at same t, then by primitive index
        if (is_exit != other.is_exit)
            return !is_exit;  // Entries (false) before exits (true)
        return prim_idx < other.prim_idx;
    }
};

}  // namespace device
}  // namespace thesis
