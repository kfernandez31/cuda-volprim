#pragma once

#include "thesis/device/params/environment_map.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/cuda/texture.h"
#include "thesis/host/utils/check.h"
#include "thesis/host/utils/io.h"

#include <cuda_runtime.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <spdlog/spdlog.h>

namespace thesis {
namespace host {
namespace params {

// Host-side wrapper with RAII texture management
class EnvironmentMap {
   private:
    cuda::CudaTexture texture_;
    device::params::EnvironmentMap device_env_map_;
    size_t num_channels_ = 0;

   public:
    // Constructor for loading HDR environment map
    EnvironmentMap(const std::filesystem::path& filepath, CUcontext ctx,
                   std::shared_ptr<cuda::Stream> stream) {
        size_t width, height;
        auto hdr = utils::try_unwrap_or_exit(
            utils::io::loadHDRImage(filepath, width, height, num_channels_));

        spdlog::info("Successfully loaded environment map '{}': {}x{}x{}", filepath.string(), width,
                     height, num_channels_);

        const auto total_floats = width * height * num_channels_;
        texture_ = cuda::CudaTexture::createRGBA({hdr.get(), total_floats}, width, height,
                                                 num_channels_, ctx, stream);

        // Store the texture handle for device use
        device_env_map_.tex_obj_ = texture_.get();

        spdlog::debug(
            "Created CUDA texture object for environment map (bilinear filtering enabled)");
    }

    // Non-copyable, moveable
    EnvironmentMap(const EnvironmentMap&) = delete;
    EnvironmentMap& operator=(const EnvironmentMap&) = delete;
    EnvironmentMap(EnvironmentMap&&) noexcept = default;
    EnvironmentMap& operator=(EnvironmentMap&&) noexcept = default;

    // Get device-compatible struct for launch params
    [[nodiscard]] const device::params::EnvironmentMap& device_env_map() const noexcept {
        return device_env_map_;
    }
};

}  // namespace params
}  // namespace host
}  // namespace thesis
