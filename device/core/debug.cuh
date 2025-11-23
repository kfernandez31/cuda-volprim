#pragma once

#include <optix.h>
#include <vector_types.h>

namespace thesis {
namespace device {

__device__ __forceinline__ bool is_debug_thread() {
    static constexpr bool debug_on = true;
    if constexpr (!debug_on) {
        return false;
    }

    const uint3 launch_index = optixGetLaunchIndex();
    const uint3 launch_dim   = optixGetLaunchDimensions();

    // Debug a pixel on the right edge where the blue outline appears
    // About 60% across horizontally, middle vertically
    const uint32_t target_x = launch_dim.x * 6 / 10;
    const uint32_t target_y = launch_dim.y / 2;

    return (launch_index.x == target_x) && (launch_index.y == target_y);
}

} // namespace device
} // namespace thesis
