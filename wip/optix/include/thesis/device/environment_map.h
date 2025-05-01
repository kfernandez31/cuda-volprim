#pragma once

#include <vector_types.h>

#include <cstddef>

// TODO: make more elegant
#ifdef __CUDACC__
#include <math.h>
// #include <math_constants.h>

#ifndef PI
// TODO: make more elegant
#define PI    3.14159265358979323846f
#define INVPI (1.0f / PI)
#endif // PI

#endif // __CUDACC__

namespace thesis {
namespace device {

struct EnvironmentMap {
    float* data = nullptr;
    float3 fallback_bg_color = {};
    size_t width = 0;
    size_t height = 0;
    size_t num_channels = 0;

#ifdef __CUDACC__
    __device__ float3 sample(const float3& dir) const {
        if (!data) return fallback_bg_color;

        const auto theta = atan2f(dir.z, dir.x);
        const auto phi   = acosf(fminf(fmaxf(dir.y, -1.0f), 1.0f));

        const auto u = (theta + PI) * (0.5f * INVPI);
        const auto v = phi * INVPI;

        const auto x = static_cast<size_t>(u * width) % width;
        const auto y = static_cast<size_t>(v * height) % height;
        const auto idx = (y * width + x) * num_channels;

        return make_float3(data[idx], data[idx + 1], data[idx + 2]);
    }
#endif // __CUDACC__

};

} // namespace device
} // namespace thesis
