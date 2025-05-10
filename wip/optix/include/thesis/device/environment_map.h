#pragma once

#include <vector_types.h>

#include <cstddef>

#ifdef __CUDACC__
#include "thesis/utils/math.h"

#include <math.h>
#endif  // __CUDACC__

namespace thesis {
namespace device {

struct alignas(16) EnvironmentMap {
    float* data_ = nullptr;
    float3 fallback_bg_color_ = {};
    size_t width_ = 0;
    size_t height_ = 0;
    size_t num_channels_ = 0;

#ifdef __CUDACC__
    __device__ float3 sample(float3 dir) const noexcept {
        if (!data_)
            return fallback_bg_color_;

        const auto theta = atan2f(dir.z, dir.x);
        const auto phi = acosf(math::clamp(dir.y, -1.0f, 1.0f));

        const auto u = (theta + math::PI_F) * math::HALF_INVPI_F;
        const auto v = phi * math::INVPI_F;

        const auto x = static_cast<size_t>(u * width_) % width_;
        const auto y = static_cast<size_t>(v * height_) % height_;
        const auto idx = (y * width_ + x) * num_channels_;

        return make_float3(data_[idx], data_[idx + 1], data_[idx + 2]);
    }
#endif  // __CUDACC__
};

}  // namespace device
}  // namespace thesis
