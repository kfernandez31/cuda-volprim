#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <vector_types.h>

#include <cstddef>

namespace thesis {
namespace device {
namespace params {

struct THESIS_ALIGNMENT Image {
    float4* sample_buffer_ = nullptr;  // Using float4 for vectorized access (w component unused)
    size_t width_ = 0;
    size_t height_ = 0;
    size_t image_size_ = 0;  // Precomputed: width * height (saves 1 multiply per ray)
    size_t num_samples_per_pixel_ = 0;

#ifdef DEVICE
    __device__ __forceinline__ size_t getGlobalSampleIndex(uint3 launch_idx) const {
        // Compute: z * image_size + y * width + x
        // Precomputed image_size saves 1 multiplication per primary ray
        return launch_idx.z * image_size_ + launch_idx.y * width_ + launch_idx.x;
    }

    __device__ float4& operator[](size_t global_idx) { return sample_buffer_[global_idx]; }

    __device__ const float4& operator[](size_t global_idx) const {
        return sample_buffer_[global_idx];
    }
#endif  // DEVICE
};

}  // namespace params
}  // namespace device
}  // namespace thesis
