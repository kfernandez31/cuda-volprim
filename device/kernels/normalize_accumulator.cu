#include "kernels/normalize_accumulator.h"
#include "thesis/common/utils/math.h"

#include <vector_types.h>

#include <cstddef>

namespace thesis {
namespace device {
namespace kernels {

// Maximum number of blocks to launch (enough to saturate 128 SMs × 8 blocks/SM)
constexpr size_t MAX_GRID_BLOCKS = 1024;
constexpr size_t BLOCK_SIZE = 256;

static __global__ void normalize_accumulator_kernel(float4* out_pixels, const float4* accumulator,
                                                    size_t image_size, float normalization_factor) {
    const size_t tid = blockIdx.x * blockDim.x + threadIdx.x;
    const size_t stride = blockDim.x * gridDim.x;

    // Grid-stride loop: each thread processes multiple pixels (coalesced memory access)
    for (size_t pixel_index = tid; pixel_index < image_size; pixel_index += stride) {
        out_pixels[pixel_index] = __ldg(&accumulator[pixel_index]) * normalization_factor;
    }
}

extern "C" void launch_normalize_accumulator_kernel(float4* out_pixels, const float4* accumulator,
                                                    size_t image_size, float normalization_factor,
                                                    cudaStream_t stream) {
    const size_t num_blocks =
        common::math::min(common::math::ceil_div(image_size, BLOCK_SIZE), MAX_GRID_BLOCKS);

    normalize_accumulator_kernel<<<num_blocks, BLOCK_SIZE, 0, stream>>>(
        out_pixels, accumulator, image_size, normalization_factor);
}

}  // namespace kernels
}  // namespace device
}  // namespace thesis
