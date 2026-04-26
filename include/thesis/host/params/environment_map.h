#pragma once

#include "thesis/common/utils/math.h"
#include "thesis/device/params/environment_map.h"
#include "thesis/host/cuda/async_buffer.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/cuda/texture.h"
#include "thesis/host/utils/check.h"
#include "thesis/host/utils/io.h"

#include <cuda_runtime.h>

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <spdlog/spdlog.h>
#include <vector>

namespace thesis {
namespace host {
namespace params {

// Host-side wrapper with RAII texture management
class EnvironmentMap {
   private:
    cuda::CudaTexture texture_;
    cuda::AsyncBuffer<float> marginal_cdf_buf_;     // size = cdf_height
    cuda::AsyncBuffer<float> conditional_cdf_buf_;  // size = cdf_height * cdf_width
    cuda::AsyncBuffer<float> joint_density_buf_;    // size = cdf_height * cdf_width
    device::params::EnvironmentMap device_env_map_;
    size_t num_channels_ = 0;

    // Build the 2D CDF used by env_is for importance sampling.
    // Lat-long convention: v ∈ [0, h] maps to polar angle θ ∈ [0, π], u ∈ [0, w] to
    // azimuth φ ∈ [-π, π]. Pixels are weighted by sin(θ) to account for solid-angle
    // foreshortening at the poles. Luminance uses Rec. 709.
    struct CdfData {
        std::vector<float> joint_density;
        std::vector<float> marginal_cdf;
        std::vector<float> conditional_cdf;
        float total_integral = 0.0f;
    };

    static CdfData buildCdf(const float* data, size_t width, size_t height, size_t channels) {
        CdfData cdf;
        cdf.joint_density.assign(width * height, 0.0f);
        cdf.conditional_cdf.assign(width * height, 0.0f);
        cdf.marginal_cdf.assign(height, 0.0f);

        for (size_t v = 0; v < height; ++v) {
            const float theta = (static_cast<float>(v) + 0.5f) / static_cast<float>(height) *
                                common::math::PI_F;
            const float sin_theta = std::sin(theta);

            float row_sum = 0.0f;
            for (size_t u = 0; u < width; ++u) {
                const auto idx = (v * width + u) * channels;
                const float r = (channels >= 1) ? data[idx + 0] : 0.0f;
                const float g = (channels >= 2) ? data[idx + 1] : r;
                const float b = (channels >= 3) ? data[idx + 2] : r;
                const float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                const float weighted = lum * sin_theta;
                cdf.joint_density[v * width + u] = weighted;
                row_sum += weighted;
                cdf.conditional_cdf[v * width + u] = row_sum;
            }
            // Normalize each row's CDF to end at 1; uniform fallback for empty rows.
            if (row_sum > 0.0f) {
                const float inv = 1.0f / row_sum;
                for (size_t u = 0; u < width; ++u) {
                    cdf.conditional_cdf[v * width + u] *= inv;
                }
            } else {
                for (size_t u = 0; u < width; ++u) {
                    cdf.conditional_cdf[v * width + u] =
                        (static_cast<float>(u) + 1.0f) / static_cast<float>(width);
                }
            }
            cdf.marginal_cdf[v] = row_sum;
        }

        // Marginal CDF: prefix sum, then normalize.
        float total = 0.0f;
        for (size_t v = 0; v < height; ++v) {
            total += cdf.marginal_cdf[v];
            cdf.marginal_cdf[v] = total;
        }
        cdf.total_integral = total;
        if (total > 0.0f) {
            const float inv = 1.0f / total;
            for (size_t v = 0; v < height; ++v) {
                cdf.marginal_cdf[v] *= inv;
            }
        } else {
            for (size_t v = 0; v < height; ++v) {
                cdf.marginal_cdf[v] = (static_cast<float>(v) + 1.0f) / static_cast<float>(height);
            }
        }
        return cdf;
    }

   public:
    // Constructor for loading HDR environment map from async future
    EnvironmentMap(std::future<utils::Result<utils::io::HDRImageData>>&& hdr_future, CUcontext ctx,
                   std::shared_ptr<cuda::Stream> stream) {
        // Complete the async loading (file I/O + memcpy to pinned happened on background thread)
        auto hdr_data = utils::try_unwrap_or_exit(hdr_future.get());

        const size_t width = hdr_data.width;
        const size_t height = hdr_data.height;
        num_channels_ = hdr_data.channels;

        spdlog::info("Successfully loaded environment map: {}x{}x{}", width, height, num_channels_);

        const auto total_floats = width * height * num_channels_;
        // Data is already in pinned memory, so cudaMemcpy2DToArrayAsync will be truly async
        texture_ = cuda::CudaTexture::createRGBA({hdr_data.data.get(), total_floats}, width, height,
                                                 num_channels_, ctx, stream);

        // Build importance sampling CDFs from the HDR data (host) and upload to device.
        const auto cdf = buildCdf(hdr_data.data.get(), width, height, num_channels_);
        marginal_cdf_buf_ = cuda::AsyncBuffer<float>(std::span<const float>(cdf.marginal_cdf), ctx,
                                                     stream, cuda::AllocType::OnDeviceOnly);
        conditional_cdf_buf_ = cuda::AsyncBuffer<float>(std::span<const float>(cdf.conditional_cdf),
                                                        ctx, stream,
                                                        cuda::AllocType::OnDeviceOnly);
        joint_density_buf_ = cuda::AsyncBuffer<float>(std::span<const float>(cdf.joint_density),
                                                       ctx, stream,
                                                       cuda::AllocType::OnDeviceOnly);

        // Store the texture handle and CDF pointers for device use
        device_env_map_.tex_obj_ = texture_.get();
        device_env_map_.marginal_cdf_ = marginal_cdf_buf_.device();
        device_env_map_.conditional_cdf_ = conditional_cdf_buf_.device();
        device_env_map_.joint_density_ = joint_density_buf_.device();
        device_env_map_.cdf_width_ = static_cast<uint32_t>(width);
        device_env_map_.cdf_height_ = static_cast<uint32_t>(height);
        device_env_map_.total_integral_ = cdf.total_integral;

        spdlog::debug("Env CDF built: {}×{}, total integral = {:.4g}", width, height,
                      cdf.total_integral);
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
