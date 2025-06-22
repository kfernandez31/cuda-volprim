#pragma once

#include <vector_types.h>

#include <cstddef>

namespace thesis {
namespace device {
namespace kernels {

extern "C" void launch_average_samples_kernel(float3* out_img, const float3* in_buf, size_t width,
                                              size_t height, size_t num_samples_per_pixel,
                                              cudaStream_t stream);
}  // namespace kernels
}  // namespace device
}  // namespace thesis
