#pragma once

#include "thesis/cuda/buffer.h"
#include "thesis/device/image.h"

#include <cstddef>

namespace thesis {
namespace host {

class Image {
   private:
    size_t width_ = 0;
    size_t height_ = 0;
    cuda::Buffer<float3> buffer_;

   public:
    Image(size_t width, float aspect_ratio)
        : width_(width),
          height_(static_cast<size_t>(static_cast<float>(width) / aspect_ratio)),
          buffer_(width_ * height_) {}

    // Disable copy
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    // Enable move
    Image(Image&&) noexcept = default;
    Image& operator=(Image&&) noexcept = default;

    [[nodiscard]] float3* host() noexcept { return buffer_.host(); }
    [[nodiscard]] const float3* host() const noexcept { return buffer_.host(); }

    [[nodiscard]] size_t width() const noexcept { return width_; }
    [[nodiscard]] size_t height() const noexcept { return height_; }

    [[nodiscard]] size_t size() const noexcept { return width_ * height_; }
    [[nodiscard]] float aspect_ratio() const noexcept {
        return static_cast<float>(width_) / static_cast<float>(height_);
    }

    [[nodiscard]] device::Image toDevice() noexcept {
        device::Image result;
        result.data_ = buffer_.device();
        result.width_ = width_;
        result.height_ = height_;
        return result;
    }

    void download() { buffer_.download(); }
};

}  // namespace host
}  // namespace thesis
