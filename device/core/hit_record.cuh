#pragma once

#include <cstdint>

namespace thesis {
namespace device {

// Unified structure for ray-primitive intersections (entries and exits)
// Used for both:
// 1. Hit collection during ray tracing (is_exit = false, from anyhit shader)
// 2. Event processing in escape case (is_exit = true for exits, false for entries)
//
// Memory: 12 bytes (float + uint + bool with padding)
// Previous design used separate HitRecord (8 bytes) and Event (12 bytes) = 20 bytes total
struct HitRecord {
    float t_hit;        // 4 bytes - distance along ray
    uint32_t prim_idx;  // 4 bytes - primitive index
                        // TODO: make size_t, but then let's have a typedef for the primitive index/count type based on upper bounds of primitive count that we expect to have to render
    bool is_exit;       // 1 byte (padded to 4) - false for entry, true for exit
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
