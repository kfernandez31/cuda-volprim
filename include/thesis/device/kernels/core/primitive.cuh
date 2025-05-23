#pragma once

#include "thesis/device/kernels/core/launch_params.cuh"
#include "thesis/common/params/launch_params.h"
#include "thesis/common/utils/types.h"

#include <optix.h>

namespace thesis {
namespace device {

// TODO(kacper): think of whether this should return a size_t
__forceinline__ __device__ uint getPrimitiveIndex() {
    const auto triangle_idx = optixGetPrimitiveIndex();
    const auto prim_idx = triangle_idx / params.num_triangles_per_primitive_;
    return prim_idx;
}

} // namespace device
} // namespace thesis
