#pragma once

#include <vector_types.h>

#include <cstddef>

namespace thesis {
namespace device {
namespace kernels {

extern "C" void launch_normalize_accumulator_kernel(float3* out_pixels, const float3* accumulator,
                                                    size_t image_size, float normalization_factor,
                                                    cudaStream_t stream);

}  // namespace kernels
}  // namespace device
}  // namespace thesis
