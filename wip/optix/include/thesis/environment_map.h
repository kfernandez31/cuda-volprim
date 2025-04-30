#pragma once

#include "thesis/utils/check.h"
#include "thesis/preprocessor.h"

#include <optix.h>
#include <cuda_runtime.h>
#include <vector_types.h>

#include <cstddef>

//TODO: clean up
#ifdef __CUDACC__
// #include <math_constants.h>
#else
#include <string_view>
#include <stb/stb_image.h>
#endif // __CUDACC__

// TODO: make more elegant
constexpr float PI    = 3.14159265358979323846f;
constexpr float INVPI = 1.0f / PI;

namespace thesis {

struct DeviceEnvironmentMap {
    float* data = nullptr;
    float3 fallback_bg_color = make_float3(0.0f, 0.0f, 0.0f);
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

#ifndef __CUDACC__
struct HostEnvironmentMap {
    cuda::UploadBuffer<float> device_data;
    size_t width = 0;
    size_t height = 0;
    size_t num_channels = 0;
    float3 fallback_bg_color = make_float3(0.0f, 0.0f, 0.0f);

    HostEnvironmentMap() = default;
    explicit HostEnvironmentMap(std::string_view filepath) {
        stbi_set_flip_vertically_on_load(true);

        int w, h, c;
        auto* host_data = stbi_loadf(filepath.data(), &w, &h, &c, 0);
        CHECK_NOT_NULL(host_data, "Failed to load HDR environment map");

        width = static_cast<size_t>(w);
        height = static_cast<size_t>(h);
        num_channels = static_cast<size_t>(c);

        const auto total_floats = width * height * num_channels;
        device_data = cuda::UploadBuffer<float>(host_data, total_floats);

        stbi_image_free(host_data);
    }

    DeviceEnvironmentMap toDevice() const noexcept {
        return DeviceEnvironmentMap{
            .data = reinterpret_cast<float*>(device_data.get()),
            .fallback_bg_color = fallback_bg_color,
            .width = width,
            .height = height,
            .num_channels = num_channels
        };
    }
};
#endif // __CUDACC__

} // namespace thesis