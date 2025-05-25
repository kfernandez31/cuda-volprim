// #include "device/kernels/pch.cuh"
#include "kernels/average_samples.h"

#include <vector_types.h>
#include <cstddef>
#include <vec_math.h>
// #include <cuda_runtime.h>

#include "thesis/common/utils/math.h"
#include "thesis/common/utils/types.h"

namespace thesis {
namespace device {

__global__ void average_samples_kernel(
    float3* out_img,
    const float3* in_buf,
    size_t width,
    size_t height,
    size_t num_samples_per_pixel
) {
    const size_t x = blockIdx.x * blockDim.x + threadIdx.x;
    const size_t y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height)
        return;

    const size_t pixel_index = y * width + x;
    const size_t image_size = width * height;

    auto acc = make_float3(0.0f);

    for (size_t s = 0; s < num_samples_per_pixel; ++s) {
        acc += in_buf[s * image_size + pixel_index];
    }

    out_img[pixel_index] = acc / static_cast<float>(num_samples_per_pixel);
}

void launch_average_samples_kernel(
    float3* out_img,
    const float3* in_buf,
    size_t width,
    size_t height,
    size_t num_samples_per_pixel,
    cudaStream_t stream
) {
    // TODO(kacper): select experimentally
    const dim3 block(16, 16);
    const dim3 grid(math::ceil_div(static_cast<uint>(width), block.x), math::ceil_div(static_cast<uint>(width), block.y));

    average_samples_kernel<<<grid, block, 0, stream>>>(out_img, in_buf, width, height, num_samples_per_pixel);
}

} // namespace device
} // namespace thesis
