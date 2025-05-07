#pragma once

#include "thesis/device/environment_map.h"
#include "thesis/utils/check.h"
#include "thesis/cuda/buffer.h"

#include <vector_types.h>

#include <cstddef>
#include <stb/stb_image.h>
#include <string_view>

namespace thesis {
namespace host {

#ifndef PI
// TODO(kacper): make more elegant
#define PI 3.14159265358979323846f
#define INVPI (1.0f / PI)
#endif  // PI

class EnvironmentMap {
private:
    size_t width_ = 0;
    size_t height_ = 0;
    size_t num_channels_ = 0;
    cuda::Buffer<float> device_data_;
    
    float3 fallback_bg_color_ = {};
public:
    explicit EnvironmentMap(std::string_view filepath) {
        stbi_set_flip_vertically_on_load(true);

        int w, h, c;
        auto* host_data = stbi_loadf(filepath.data(), &w, &h, &c, 0);
        CHECK_NOT_NULL(host_data, "Failed to load HDR environment map");

        width_ = static_cast<size_t>(w);
        height_ = static_cast<size_t>(h);
        num_channels_ = static_cast<size_t>(c);

        const auto total_floats = width_ * height_ * num_channels_;
        device_data_ = cuda::Buffer<float>(host_data, total_floats);

        device_data_ = cuda::Buffer<float>::onDeviceOnly(host_data, total_floats);
        stbi_image_free(host_data);
    }

    // Disable copy
    EnvironmentMap(const EnvironmentMap&) = delete;
    EnvironmentMap& operator=(const EnvironmentMap&) = delete;

    // Enable move
    EnvironmentMap(EnvironmentMap&&) noexcept = default;
    EnvironmentMap& operator=(EnvironmentMap&&) noexcept = default;

    [[nodiscard]] device::EnvironmentMap toDevice() noexcept {
        return device::EnvironmentMap{
            reinterpret_cast<float*>(device_data_.device()),
            fallback_bg_color_,
            width_,
            height_,
            num_channels_
        };
    }
};

}  // namespace host
}  // namespace thesis
