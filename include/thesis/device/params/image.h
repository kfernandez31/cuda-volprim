#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <vector_types.h>

#include <cstddef>

namespace thesis {
namespace device {

struct THESIS_ALIGNMENT Image {
    float3* sample_buffer_ = nullptr;
    size_t width_ = 0;
    size_t height_ = 0;
    size_t num_samples_per_pixel_ = 0;

#ifdef __CUDACC__

    __forceinline__ __device__ size_t getGlobalSampleIndex(uint3 launch_idx) const noexcept {
        return (launch_idx.z * width_ * height_) + (launch_idx.y * width_) + launch_idx.x;
    }

    __forceinline__ __device__ float3& operator[](size_t global_idx) noexcept {
        return sample_buffer_[global_idx];
    }
    __forceinline__ __device__ const float3& operator[](size_t global_idx) const noexcept {
        return sample_buffer_[global_idx];
    }

#endif  // __CUDACC__
};

}  // namespace device
}  // namespace thesis
