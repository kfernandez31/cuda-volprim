#pragma once

#include "thesis/device/environment_map.h"
#include "thesis/utils/check.h"
#include "thesis/cuda/buffer.h"

#include <vector_types.h>

#include <cstddef>
#include <filesystem>
#include <string_view>

#include <stb/stb_image.h>

namespace thesis {
namespace host {

class EnvironmentMap {
private:
    size_t width_ = 0;
    size_t height_ = 0;
    size_t num_channels_ = 0;
    cuda::Buffer<float> device_data_;
    
    float3 fallback_bg_color_ = {};
public:
    explicit EnvironmentMap(const std::filesystem::path& filepath) {
        stbi_set_flip_vertically_on_load(true);

        int w, h, c;
        auto* host_data = stbi_loadf(filepath.string().c_str(), &w, &h, &c, 0);
        CHECK_NOT_NULL(host_data, "Failed to load HDR environment map");

        width_ = static_cast<size_t>(w);
        height_ = static_cast<size_t>(h);
        num_channels_ = static_cast<size_t>(c);

        const auto total_floats = width_ * height_ * num_channels_;
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
        device::EnvironmentMap result;
        result.data_ = device_data_.device();
        result.fallback_bg_color_ = fallback_bg_color_;
        result.width_ = width_;
        result.height_ = height_;
        result.num_channels_ = num_channels_;
        return result;
    }
};

}  // namespace host
}  // namespace thesis
