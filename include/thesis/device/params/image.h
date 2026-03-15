#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <vector_types.h>

#include <cstddef>
#include <cstdint>

namespace thesis {
namespace device {
namespace params {

// Device-side POD struct for image buffer (no RAII, same size on host and device)
struct THESIS_ALIGNMENT Image {
    float4* variance_ = nullptr;        // Running M2 (sum of squared deviations) for Welford's algorithm
    float4* mean_ = nullptr;            // Running mean for Welford's algorithm
    size_t* sample_counts_ = nullptr;   // Number of samples taken per pixel (size_t for buffer compatibility)
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t num_samples_per_pixel_ = 0;  // Total samples (for final normalization)
    uint32_t batch_offset_ = 0;           // Starting sample index for current batch
    uint32_t batch_size_ = 0;             // Number of samples in current batch

    Image() = default;
    Image(const Image&) = default;
    Image& operator=(const Image&) = default;
};

}  // namespace params
}  // namespace device
}  // namespace thesis
