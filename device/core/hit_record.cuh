#pragma once

#include "thesis/common/utils/types.h"

#include <cstdint>

namespace thesis {
namespace device {

// Unified structure for ray-primitive intersections (entries and exits)
// Used for both:
// 1. Hit collection during ray tracing (is_exit = false, from anyhit shader)
// 2. Event processing in escape case (is_exit = true for exits, false for entries)
//
// Memory: 8 bytes (float + uint16 + bool with padding)
struct HitRecord {
    float t_hit;               // 4 bytes - distance along ray
    prim_idx_t prim_idx;  // 2 bytes - primitive index
    bool is_exit;              // 1 byte - false for entry, true for exit
                        // Note: Always false during hit collection (anyhit shader)
                        //       Only used in escape case event processing

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
