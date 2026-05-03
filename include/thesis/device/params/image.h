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
    float4* variance_ = nullptr;  // Welford M2; null when adaptive sampling is disabled (skip writes)
    float4* mean_ = nullptr;      // Running mean for Welford's algorithm
    uint16_t* sample_counts_ =
        nullptr;  // Per-pixel sample count (max 65535 spp; widen if you ever need more)

    // Auxiliary outputs ("AOVs") for the OptiX denoiser as guide layers.
    // Both are running per-pixel means across SPP (no Welford M2 needed — we just need
    // the average for the denoiser's prior). Written at bounce 0 only.
    //   albedo_aov_: scatter-point albedo at first scatter, or 0 for unscattered paths.
    //   normal_aov_: -ray.direction at the first bounce (camera-facing pseudo-normal).
    float4* albedo_aov_ = nullptr;
    float4* normal_aov_ = nullptr;

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
