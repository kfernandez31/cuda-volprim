#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <vector_types.h>

#include <cstddef>

namespace thesis {
namespace device {
namespace params {

// Device-side POD struct for image buffer (no RAII, same size on host and device)
struct THESIS_ALIGNMENT Image {
    float4* sample_buffer_ = nullptr;  // Using float4 for vectorized access (w component unused)
    float4* variance_ = nullptr;       // Running M2 (sum of squared deviations) for Welford's algorithm
    float4* mean_ = nullptr;           // Running mean for Welford's algorithm
    size_t* sample_counts_ = nullptr;  // Number of samples taken per pixel
    size_t width_ = 0;
    size_t height_ = 0;
    size_t image_size_ = 0;             // Precomputed: width * height (saves 1 multiply per ray)
    size_t num_samples_per_pixel_ = 0;  // Total samples (for final normalization)
    size_t batch_offset_ = 0;           // Starting sample index for current batch
    size_t batch_size_ = 0;             // Number of samples in current batch

    Image() = default;
    Image(const Image&) = default;
    Image& operator=(const Image&) = default;

#ifdef DEVICE
    // Device-only: compute global sample index from 3D launch index
    __device__ __forceinline__ size_t getGlobalSampleIndex(uint3 launch_idx) const {
        // Compute: z * image_size + y * width + x
        // Precomputed image_size saves 1 multiplication per primary ray
        return launch_idx.z * image_size_ + launch_idx.y * width_ + launch_idx.x;
    }

    // Device-only: array access operators
    __device__ float4& operator[](size_t global_idx) { return sample_buffer_[global_idx]; }

    __device__ const float4& operator[](size_t global_idx) const {
        return sample_buffer_[global_idx];
    }
#endif  // DEVICE
};

}  // namespace params
}  // namespace device
}  // namespace thesis
