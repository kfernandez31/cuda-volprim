#pragma once

#include "kernels/average_samples.h"
#include "thesis/common/utils/math.h"
#include "thesis/device/params/image.h"
#include "thesis/host/cuda/async_buffer.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/utils/check.h"
#include "thesis/host/utils/io.h"
#include "thesis/host/utils/result.h"

#include <vector_types.h>

#include <cstddef>
#include <filesystem>
#include <future>
#include <memory>
#include <spdlog/spdlog.h>

namespace thesis {
namespace host {
namespace params {

// Host-side wrapper with RAII buffer management
class Image {
   private:
    cuda::AsyncBuffer<float4> sample_buffer_managed_;    // Sample-major: [s*H*W + y*W + x]
    cuda::AsyncBuffer<float3> averaged_pixels_managed_;  // Single-layer output
    device::params::Image device_image_;                 // Device-compatible POD struct

   public:
    // Constructor for buffer allocation
    Image(size_t width, size_t height, size_t num_samples_per_pixel, CUcontext ctx,
          std::shared_ptr<cuda::Stream> sample_buffer_stream,
          std::shared_ptr<cuda::Stream> averaged_pixels_stream)
        : sample_buffer_managed_(width * height * num_samples_per_pixel, ctx,
                                 std::move(sample_buffer_stream), cuda::AllocType::OnDeviceOnly),
          averaged_pixels_managed_(width * height, ctx, std::move(averaged_pixels_stream),
                                   cuda::AllocType::OnBoth) {
        device_image_.sample_buffer_ = const_cast<float4*>(sample_buffer_managed_.device());
        device_image_.width_ = width;
        device_image_.height_ = height;
        device_image_.image_size_ = width * height;
        device_image_.num_samples_per_pixel_ = num_samples_per_pixel;
    }

    // Non-copyable, moveable
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&&) noexcept = default;
    Image& operator=(Image&&) noexcept = default;

    // Get device-compatible struct for launch params
    [[nodiscard]] const device::params::Image& device_image() const noexcept {
        return device_image_;
    }

    // Utility methods
    [[nodiscard]] size_t width() const noexcept { return device_image_.width_; }
    [[nodiscard]] size_t height() const noexcept { return device_image_.height_; }
    [[nodiscard]] size_t num_samples_per_pixel() const noexcept {
        return device_image_.num_samples_per_pixel_;
    }

    [[nodiscard]] size_t pixel_count() const noexcept { return device_image_.image_size_; }
    [[nodiscard]] size_t total_size() const noexcept {
        return device_image_.width_ * device_image_.height_ * device_image_.num_samples_per_pixel_;
    }

    [[nodiscard]] float aspect_ratio() const noexcept {
        return static_cast<float>(device_image_.width_) *
               common::math::rcp(static_cast<float>(device_image_.height_));
    }

    // Save averaged image to EXR file
    [[nodiscard]] std::future<utils::Result<>> save(const std::filesystem::path& filename) {
        const auto& stream = averaged_pixels_managed_.get_context_param();

        spdlog::info("Averaging {} samples per pixel ({}x{})", device_image_.num_samples_per_pixel_,
                     device_image_.width_, device_image_.height_);

        device::kernels::launch_average_samples_kernel(
            averaged_pixels_managed_.device(), sample_buffer_managed_.device(),
            device_image_.width_, device_image_.height_, device_image_.num_samples_per_pixel_,
            stream->get());

        averaged_pixels_managed_.download();
        stream->synchronize();

        spdlog::info("Saving to '{}'", filename.string());
        // Move buffer ownership to async task (zero-copy)
        return utils::io::async::saveExr(std::move(averaged_pixels_managed_), device_image_.width_,
                                         device_image_.height_, filename);
    }
};

}  // namespace params
}  // namespace host
}  // namespace thesis
