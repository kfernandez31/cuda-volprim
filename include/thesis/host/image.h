#pragma once

#include "thesis/cuda/buffer.h"
#include "thesis/device/image.h"
#include "thesis/host/convertible.h"
#include "thesis/utils/io.h"
#include "thesis/utils/result.h"

#include <cstddef>
#include <filesystem>
#include <span>

namespace thesis {
namespace host {

// TODO(kacper): depth-layered (z-axis) many-buffer support + kernel for reduction. Zero-initialized!!!
class Image : public Convertible<device::Image> {
   private:
    size_t width_ = 0;
    size_t height_ = 0;
    cuda::Buffer<float3> buffer_;

    Image(size_t size) : buffer_(size) {}

   public:
    static Image fromWandH(size_t width, size_t height) noexcept {
        Image result(width * height);
        result.width_ = width;
        result.height_ = height;
        return result;
    }

    static Image fromWandAR(size_t width, float aspect_ratio) noexcept {
        auto height = static_cast<size_t>(static_cast<float>(width) / aspect_ratio);
        return fromWandH(width, height);
    }

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
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

    [[nodiscard]] device::Image toDevice() const noexcept override {
        device::Image result;
        result.data_ = const_cast<float3*>(buffer_.device());
        result.width_ = width_;
        result.height_ = height_;
        return result;
    }

    Result<Unit> save(const std::filesystem::path& filename) {
        buffer_.download();
        std::span<const float3> framebuffer(host(), size());
        return io::saveExrImage(framebuffer, width(), height(), filename);
    }
};

}  // namespace host
}  // namespace thesis
