#pragma once

#include "thesis/device/params/environment_map.h"
#include "thesis/host/cuda/async_buffer.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/params/convertible.h"
#include "thesis/host/utils/check.h"
#include "thesis/host/utils/io.h"

#include <vector_types.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <spdlog/spdlog.h>
#include <string_view>

namespace thesis::host::params {

class EnvironmentMap : public Convertible<device::params::EnvironmentMap> {
   private:
    size_t width_ = 0;
    size_t height_ = 0;
    size_t num_channels_ = 0;
    cuda::AsyncBuffer<float> data_;

   public:
    EnvironmentMap(const std::filesystem::path& filepath, CUcontext ctx,
                   std::shared_ptr<cuda::Stream> stream) {
        auto hdr = utils::try_unwrap_or_exit(
            utils::io::loadHDRImage(filepath, width_, height_, num_channels_));

        spdlog::info("Successfully loaded environment map '{}': {}x{}x{}", filepath.string(),
                     width_, height_, num_channels_);

        const auto total_floats = width_ * height_ * num_channels_;
        data_ = cuda::AsyncBuffer<float>({hdr.get(), total_floats}, ctx, std::move(stream),
                                         cuda::AllocType::OnBoth);
    }

    EnvironmentMap(EnvironmentMap&&) noexcept = default;
    EnvironmentMap& operator=(EnvironmentMap&&) noexcept = default;

    EnvironmentMap(const EnvironmentMap&) = delete;
    EnvironmentMap& operator=(const EnvironmentMap&) = delete;

    [[nodiscard]] device::params::EnvironmentMap toDevice() const noexcept override {
        device::params::EnvironmentMap result;
        result.data_ = const_cast<float*>(data_.device());
        result.width_ = width_;
        result.height_ = height_;
        result.num_channels_ = num_channels_;
        return result;
    }
};

}  // namespace thesis::host::params
