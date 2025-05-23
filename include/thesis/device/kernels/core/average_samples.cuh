#pragma once

#include <vector_types.h>
#include <cstddef>

namespace thesis {
namespace device {

__global__ void average_samples_kernel(
    float3* out_img,
    const float3* in_buf,
    size_t width,
    size_t height,
    size_t num_samples_per_pixel
);

} // namespace device
} // namespace thesis
