#pragma once

#include "thesis/device/params/environment_map.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/cuda/texture.h"
#include "thesis/host/params/convertible.h"
#include "thesis/host/utils/check.h"
#include "thesis/host/utils/io.h"

#include <vector_types.h>

#include <cstddef>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <utility>

namespace thesis::host::params {

class EnvironmentMap : public Convertible<device::params::EnvironmentMap> {
   private:
    cuda::CudaTexture texture_;
    size_t num_channels_ = 0;

   public:
    EnvironmentMap(const std::filesystem::path& filepath, CUcontext ctx,
                   std::shared_ptr<cuda::Stream> stream) {
        size_t width, height;
        auto hdr = utils::try_unwrap_or_exit(
            utils::io::loadHDRImage(filepath, width, height, num_channels_));

        spdlog::info("Successfully loaded environment map '{}': {}x{}x{}", filepath.string(),
                     width, height, num_channels_);

        const auto total_floats = width * height * num_channels_;
        texture_ = cuda::CudaTexture::createRGBA(
            {hdr.get(), total_floats}, width, height, num_channels_, ctx, stream);

        spdlog::info("Created CUDA texture object for environment map (bilinear filtering enabled)");
    }

    EnvironmentMap(EnvironmentMap&&) noexcept = default;
    EnvironmentMap& operator=(EnvironmentMap&&) noexcept = default;

    [[nodiscard]] device::params::EnvironmentMap toDevice() const noexcept override {
        device::params::EnvironmentMap result;
        result.tex_obj_ = texture_.get();
        return result;
    }
};

}  // namespace thesis::host::params
