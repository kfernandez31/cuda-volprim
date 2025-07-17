#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <vector_types.h>

#include <cstddef>

namespace thesis {
namespace device {
namespace params {

struct THESIS_ALIGNMENT Image {
    float3* sample_buffer_ = nullptr;
    size_t width_ = 0;
    size_t height_ = 0;
    size_t num_samples_per_pixel_ = 0;

#ifdef DEVICE
    __device__ size_t getGlobalSampleIndex(uint3 launch_idx) const {
        return (launch_idx.z * width_ * height_) + (launch_idx.y * width_) + launch_idx.x;
    }

    __device__ float3& operator[](size_t global_idx) { return sample_buffer_[global_idx]; }

    __device__ const float3& operator[](size_t global_idx) const {
        return sample_buffer_[global_idx];
    }
#endif  // DEVICE
};

}  // namespace params
}  // namespace device
}  // namespace thesis
