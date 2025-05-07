#pragma once

#include <vector_types.h>

#include <cstddef>

// TODO(kacper): make more elegant
#ifdef __CUDACC__
#include <math.h>
// #include <math_constants.h>

#ifndef PI
// TODO: make more elegant
#define PI 3.14159265358979323846f
#define INVPI (1.0f / PI)
#endif  // PI

#endif  // __CUDACC__

namespace thesis {
namespace device {

struct EnvironmentMap {
    float* data_ = nullptr;
    float3 fallback_bg_color_ = {};
    size_t width_ = 0;
    size_t height_ = 0;
    size_t num_channels_ = 0;

#ifdef __CUDACC__
    __device__ float3 sample(const float3& dir) const {
        if (!data_)
            return fallback_bg_color_;

        const auto theta = atan2f(dir.z, dir.x);
        const auto phi = acosf(fminf(fmaxf(dir.y, -1.0f), 1.0f));

        const auto u = (theta + PI) * (0.5f * INVPI);
        const auto v = phi * INVPI;

        const auto x = static_cast<size_t>(u * width_) % width_;
        const auto y = static_cast<size_t>(v * height_) % height_;
        const auto idx = (y * width_ + x) * num_channels_;

        return make_float3(data_[idx], data_[idx + 1], data_[idx + 2]);
    }
#endif  // __CUDACC__
};

}  // namespace device
}  // namespace thesis
