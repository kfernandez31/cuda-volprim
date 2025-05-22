#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <vector_types.h>
#include <cstddef>

namespace thesis {
namespace device {

struct THESIS_ALIGNMENT Image {
    float3* data_ = nullptr;
    size_t width_ = 0;
    size_t height_ = 0;

#ifdef __CUDACC__
    __forceinline__ __device__ float3* operator[](size_t y) noexcept {
        return data_ + y * width_;
    }

    __forceinline__ __device__ const float3* operator[](size_t y) const noexcept {
        return data_ + y * width_;
    }
#endif  // __CUDACC__
};

}  // namespace device
}  // namespace thesis
