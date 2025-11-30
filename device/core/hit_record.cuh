#pragma once

#include <cstdint>

namespace thesis {
namespace device {

// Record of a single ray-primitive intersection
struct HitRecord {
    float t_hit;
    uint32_t prim_idx : 31;
    uint32_t is_exit : 1;
};

} // namespace device
} // namespace thesis
