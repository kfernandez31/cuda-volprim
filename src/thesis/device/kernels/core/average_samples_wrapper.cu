#include "thesis/device/kernels/core/average_samples.cuh"
#include "thesis/common/utils/math.h"

#include <cstddef>
#include <vector_types.h>
#include <cuda_runtime.h>

namespace thesis {
namespace host {

extern "C" void launch_average_samples_kernel(
    float3* out_img,
    const float3* in_buf,
    size_t width,
    size_t height,
    size_t num_samples_per_pixel,
    cudaStream_t stream
) {
    // TODO(kacper): select experimentally
    const dim3 block(16, 16);
    const dim3 grid(math::ceil_div(width, block.x), math::ceil_div(height, block.y));
    device::average_samples_kernel<<<grid, block, 0, stream>>>(out_img, in_buf, width, height, num_samples_per_pixel);
}

} // namespace thesis
} // namespace host
