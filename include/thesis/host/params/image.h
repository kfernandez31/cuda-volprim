#pragma once

#include "thesis/device/params/image.h"
#include "thesis/host/cuda/buffer.h"
#include "thesis/host/utils/io.h"
#include "thesis/host/cuda/stream_handle.h"
#include "thesis/host/params/convertible.h"
#include "thesis/host/utils/result.h"
#include "kernels/average_samples.h"

#include <cstddef>
#include <filesystem>

namespace thesis::host::params {

class Image : public Convertible<device::params::Image> {
   private:
    size_t width_ = 0;
    size_t height_ = 0;
    size_t num_samples_per_pixel_ = 0;
    cuda::Buffer<float3> sample_buffer_;    // Sample-major buffer: [s * H * W + y * W + x]
    cuda::Buffer<float3> averaged_pixels_;  // Single-layer output

   public:
    Image() = default;

    Image(Image&&) noexcept = default;
    Image& operator=(Image&&) noexcept = default;

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    Image(size_t width, size_t height, size_t num_samples_per_pixel)
        : width_(width),
          height_(height),
          num_samples_per_pixel_(num_samples_per_pixel),
          sample_buffer_(cuda::Buffer<float3>::onDeviceOnly(total_size())),
          averaged_pixels_(pixel_count()) {}

    [[nodiscard]] size_t width() const noexcept { return width_; }
    [[nodiscard]] size_t height() const noexcept { return height_; }
    [[nodiscard]] size_t num_samples_per_pixel() const noexcept { return num_samples_per_pixel_; }

    [[nodiscard]] size_t pixel_count() const noexcept { return width_ * height_; }
    [[nodiscard]] size_t total_size() const noexcept {
        return width_ * height_ * num_samples_per_pixel_;
    }

    [[nodiscard]] float aspect_ratio() const noexcept {
        return static_cast<float>(width_) / static_cast<float>(height_);
    }

    [[nodiscard]] device::params::Image toDevice() const noexcept override {
        device::params::Image img;
        img.sample_buffer_ = const_cast<float3*>(sample_buffer_.device());
        img.width_ = width_;
        img.height_ = height_;
        img.num_samples_per_pixel_ = num_samples_per_pixel_;
        return img;
    }

    void save(const std::filesystem::path& filename, cudaStream_t stream) {
        device::launch_average_samples_kernel(averaged_pixels_.device(), sample_buffer_.device(),
                                              width_, height_, num_samples_per_pixel_, stream);
        averaged_pixels_.download();
        // TODO(kacper): fix asap
        // core::try_unwrap_or_exit(utils::io::saveExrImage(averaged_pixels_.host_view(), width_, height_, filename));
    }    
};

}  // namespace thesis::host::params
