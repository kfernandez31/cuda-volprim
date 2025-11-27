#include "kernels/average_samples.h"
#include "thesis/common/utils/math.h"
#include "thesis/common/utils/types.h"

#include <vector_types.h>

#include <cstddef>
#include <sutil/vec_math.h>

namespace thesis {
namespace device {
namespace kernels {

static __global__ void average_samples_kernel(float3* out_img, const float4* in_buf, size_t width,
                                              size_t height, size_t num_samples_per_pixel) {
    const size_t image_size = width * height;
    const size_t tid = blockIdx.x * blockDim.x + threadIdx.x;
    const size_t stride = blockDim.x * gridDim.x;

    // Grid-stride loop: each thread processes multiple pixels
    // This improves occupancy and allows more threads to be active
    for (size_t pixel_index = tid; pixel_index < image_size; pixel_index += stride) {
        auto acc = make_float4(0.0f);

// Unroll accumulation loop for better ILP
#pragma unroll 4
        for (size_t s = 0; s < num_samples_per_pixel; ++s) {
            // Vectorized 128-bit load (guaranteed with float4)
            acc += in_buf[s * image_size + pixel_index];
        }

        const auto avg = acc / static_cast<float>(num_samples_per_pixel);
        out_img[pixel_index] = make_float3(avg.x, avg.y, avg.z);
    }
}

void launch_average_samples_kernel(float3* out_img, const float4* in_buf, size_t width,
                                   size_t height, size_t num_samples_per_pixel,
                                   cudaStream_t stream) {
    const size_t image_size = width * height;

    // Use 1D grid-stride configuration for better efficiency
    // 256 threads per block is a good balance for most GPUs
    const size_t block_size = 256;

    // Launch enough blocks to saturate GPU, but not too many
    // Heuristic: 4-8 blocks per SM is typical, modern GPUs have 80-144 SMs
    // For image_size < block_size, we only need 1 block
    const size_t num_blocks = common::math::min(
        common::math::ceil_div(image_size, block_size),
        static_cast<size_t>(1024)  // Cap at 1024 blocks (enough for 128 SMs × 8 blocks/SM)
    );

    average_samples_kernel<<<num_blocks, block_size, 0, stream>>>(out_img, in_buf, width, height,
                                                                  num_samples_per_pixel);
}

}  // namespace kernels
}  // namespace device
}  // namespace thesis
