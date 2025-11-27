#pragma once

#include "kernels/average_samples.h"
#include "thesis/device/params/image.h"
#include "thesis/host/cuda/async_buffer.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/params/convertible.h"
#include "thesis/host/utils/check.h"
#include "thesis/host/utils/io.h"
#include "thesis/host/utils/result.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <spdlog/spdlog.h>

namespace thesis::host::params {

class Image : public Convertible<device::params::Image> {
   private:
    size_t width_ = 0;
    size_t height_ = 0;
    size_t num_samples_per_pixel_ = 0;
    cuda::AsyncBuffer<float4> sample_buffer_;    // Sample-major buffer: [s * H * W + y * W + x]
                                                 // (float4 for vectorized access, w unused)
    cuda::AsyncBuffer<float3> averaged_pixels_;  // Single-layer output

   public:
    Image(Image&&) noexcept = default;
    Image& operator=(Image&&) noexcept = default;

    Image(size_t width, size_t height, size_t num_samples_per_pixel, CUcontext ctx,
          std::shared_ptr<cuda::Stream> sample_buffer_stream,
          std::shared_ptr<cuda::Stream> averaged_pixels_stream)
        : width_(width),
          height_(height),
          num_samples_per_pixel_(num_samples_per_pixel),
          sample_buffer_(total_size(), ctx, std::move(sample_buffer_stream),
                         cuda::AllocType::OnDeviceOnly),
          averaged_pixels_(pixel_count(), ctx, std::move(averaged_pixels_stream),
                           cuda::AllocType::OnBoth) {}

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
        img.sample_buffer_ = const_cast<float4*>(sample_buffer_.device());
        img.width_ = width_;
        img.height_ = height_;
        img.image_size_ = width_ * height_;
        img.num_samples_per_pixel_ = num_samples_per_pixel_;
        return img;
    }

    [[nodiscard]] utils::Result<> save(const std::filesystem::path& filename) {
        const auto& stream = averaged_pixels_.get_context_param();

        spdlog::info("Averaging {} samples per pixel ({}x{}) into EXR '{}'", num_samples_per_pixel_,
                     width_, height_, filename.string());

        device::kernels::launch_average_samples_kernel(averaged_pixels_.device(),
                                                       sample_buffer_.device(), width_, height_,
                                                       num_samples_per_pixel_, stream->get());
        spdlog::debug("Launched average_samples_kernel");

        averaged_pixels_.download();

        auto view = averaged_pixels_.host_view();
        auto w = width_;
        auto h = height_;

        spdlog::info("Saving EXR to '{}'", filename.string());
        CUDA_CHECK(
            cudaDeviceSynchronize());  // TODO: necessary? or just synchronizing the stream param
                                       // would be enough? Also, manual cudaDeviceSynchronize is
                                       // ugly, we have Stream::synchronizeDevice() for a reason
        return utils::io::saveExrImage(view, w, h, filename);
        return {};
    }
};

}  // namespace thesis::host::params
