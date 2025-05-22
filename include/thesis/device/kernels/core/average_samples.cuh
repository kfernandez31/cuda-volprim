#pragma once

#include <vector_types.h>

namespace thesis {
namespace device {

__global__ void average_samples_kernel(
    float3* out_img,
    const float3* in_buf,
    size_t width,
    size_t height,
    size_t samples_per_pixel
) {
    const size_t x = blockIdx.x * blockDim.x + threadIdx.x;
    const size_t y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height)
        return;

    const size_t pixel_index = y * width + x;
    const size_t image_size = width * height;

    auto acc = make_float3(0.0f);

    for (size_t s = 0; s < samples_per_pixel; ++s) {
        acc += in_buf[s * image_size + pixel_index];
    }

    out_img[pixel_index] = acc / static_cast<float>(samples_per_pixel);
}

} // namespace device
} // namespace thesis
