#pragma once

#ifdef __cplusplus
#ifndef __CUDACC__ // TODO: introduce in older headers as well

#include "thesis/device/environment_map.h"
#include "thesis/utils/check.h"

#include <stb/stb_image.h>

#include <vector_types.h>

#include <cstddef>
#include <string_view>

namespace thesis {
namespace host {

#ifndef PI
// TODO: make more elegant
#define PI    3.14159265358979323846f
#define INVPI (1.0f / PI)
#endif // PI

struct EnvironmentMap {
    cuda::UploadBuffer<float> device_data;
    size_t width = 0;
    size_t height = 0;
    size_t num_channels = 0;
    float3 fallback_bg_color = {};

    EnvironmentMap() = default;

    explicit EnvironmentMap(std::string_view filepath) {
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

    [[nodiscard]] device::EnvironmentMap toDevice() const noexcept {
        return device::EnvironmentMap{
            .data = reinterpret_cast<float*>(device_data.get()),
            .fallback_bg_color = fallback_bg_color,
            .width = width,
            .height = height,
            .num_channels = num_channels
        };
    }
};

#endif // __CUDACC__
#endif // __cplusplus

} // namespace host
} // namespace thesis