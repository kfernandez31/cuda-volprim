#pragma once

#include <cstdint>

namespace thesis {
namespace device {

// Record of a single ray-primitive intersection
// Used for collecting all hits along a ray before processing
struct HitRecord {
    float t_hit;      // Distance along ray
    uint32_t prim_idx;  // Primitive index (from optixGetInstanceId)
    bool is_exit;     // True if exit face, false if entry face
};

} // namespace device
} // namespace thesis
