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

    const uint32_t mid_x = launch_dim.x / 2;
    const uint32_t mid_y = launch_dim.y / 2;

    return (launch_index.x == mid_x) && (launch_index.y == mid_y);
}

} // namespace device
} // namespace thesis
