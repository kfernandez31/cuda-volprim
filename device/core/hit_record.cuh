#pragma once

#include <cstdint>

namespace thesis {
namespace device {

// Record of a single ray-primitive intersection
// Used for collecting all hits along a ray before processing
struct HitRecord {
    float t_hit;        // Distance along ray
    uint32_t prim_idx;  // Primitive index (from optixGetInstanceId)
    bool is_exit;       // True if exit hit, false if entry hit
    // Note: With backface culling, traced hits are all entries (is_exit=false)
    //       but we compute exits analytically and add them with is_exit=true
    // TODO(kacper): Optimize memory - pack is_exit as high bit of prim_idx since we likely don't use all 32 bits
    //               This would reduce struct size from 12 bytes to 8 bytes (50% reduction, better cache)
};

} // namespace device
} // namespace thesis
