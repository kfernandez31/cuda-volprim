#pragma once

#include "thesis/host/cuda/buffer.h"
#include "thesis/device/params/image.h"
#include "thesis/host/params/convertible.h"
#include "thesis/host/utils/result.h"
#include "thesis/host/cuda/stream_handle.h"

#include <cstddef>
#include <filesystem>

namespace thesis {
namespace host {

class Image : public Convertible<device::Image> {
   private:
    size_t width_ = 0;
    size_t height_ = 0;
    size_t samples_per_pixel_ = 0;
    cuda::Buffer<float3> sample_buffer_;    // Sample-major buffer: [s * H * W + y * W + x]
    cuda::Buffer<float3> averaged_pixels_;  // Single-layer output

    core::Result average_host();
    core::Result average_device(const cuda::StreamHandle& stream);
    core::Result average(const cuda::StreamHandle& stream);

   public:
    Image(size_t width, size_t height, size_t samples_per_pixel)
        : width_(width)
        , height_(height)
        , samples_per_pixel_(samples_per_pixel)
        , sample_buffer_(width * height * samples_per_pixel)
        , averaged_pixels_(width * height)
        {}

    Image() = default;

    Image(Image&&) noexcept = default;
    Image& operator=(Image&&) noexcept = default;

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    [[nodiscard]] float3* host() noexcept { return sample_buffer_.host(); }
    [[nodiscard]] const float3* host() const noexcept { return sample_buffer_.host(); }

    [[nodiscard]] size_t width() const noexcept { return width_; }
    [[nodiscard]] size_t height() const noexcept { return height_; }
    [[nodiscard]] size_t samples_per_pixel() const noexcept { return samples_per_pixel_; }

    [[nodiscard]] size_t pixel_count() const noexcept { return width_ * height_; }
    [[nodiscard]] size_t total_size() const noexcept { return width_ * height_ * samples_per_pixel_; }

    [[nodiscard]] float aspect_ratio() const noexcept { return static_cast<float>(width_) / static_cast<float>(height_); }

    [[nodiscard]] device::Image toDevice() const noexcept override {
        device::Image result;
        result.data_ = const_cast<float3*>(sample_buffer_.device());
        result.width_ = width_;
        result.height_ = height_;
        result.samples_per_pixel_ = samples_per_pixel_;
        return result;
    }

    core::Result save(const std::filesystem::path& filename, const cuda::StreamHandle& stream) noexcept;
};

}  // namespace host
}  // namespace thesis
