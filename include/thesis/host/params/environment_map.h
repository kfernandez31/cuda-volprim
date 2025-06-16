#pragma once

#include "thesis/device/params/environment_map.h"
#include "thesis/host/cuda/buffer.h"
#include "thesis/host/params/convertible.h"
#include "thesis/host/utils/check.h"

#include <vector_types.h>

#include <cstddef>
#include <filesystem>
#include <stb/stb_image.h>
#include <string_view>

namespace thesis::host::params {

class EnvironmentMap : public Convertible<device::params::EnvironmentMap> {
   private:
    size_t width_ = 0;
    size_t height_ = 0;
    size_t num_channels_ = 0;
    cuda::Buffer<float> device_data_;

   public:
    EnvironmentMap(const std::filesystem::path& filepath, CUcontext ctx) {
        stbi_set_flip_vertically_on_load(true);

        int w, h, c;
        auto* raw = stbi_loadf(filepath.string().c_str(), &w, &h, &c,
                               0);  // TODO(kacper): what's the 0 arg?
        CHECK_NOT_NULL(raw, "Failed to load HDR environment map");

        width_ = static_cast<size_t>(w);
        height_ = static_cast<size_t>(h);
        num_channels_ = static_cast<size_t>(c);

        const auto total_floats = width_ * height_ * num_channels_;
        device_data_ = cuda::Buffer<float>({raw, total_floats}, ctx, cuda::AllocType::OnBoth);
        stbi_image_free(raw);
    }

    EnvironmentMap(EnvironmentMap&&) noexcept = default;
    EnvironmentMap& operator=(EnvironmentMap&&) noexcept = default;

    EnvironmentMap(const EnvironmentMap&) = delete;
    EnvironmentMap& operator=(const EnvironmentMap&) = delete;

    [[nodiscard]] device::params::EnvironmentMap toDevice() const noexcept override {
        device::params::EnvironmentMap result;
        result.data_ = const_cast<float*>(device_data_.device());
        result.width_ = width_;
        result.height_ = height_;
        result.num_channels_ = num_channels_;
        return result;
    }
};

}  // namespace thesis::host::params
