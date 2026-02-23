#pragma once

#include "kernels/normalize_accumulator.h"
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
    cuda::AsyncBuffer<float4> variance_managed_;         // Running M2 for Welford's algorithm
    cuda::AsyncBuffer<float4> mean_managed_;             // Running mean for Welford's algorithm
    cuda::AsyncBuffer<size_t> sample_counts_managed_;    // Per-pixel sample counts
    cuda::AsyncBuffer<float4> averaged_pixels_managed_;  // Final output (RGBA, W unused)
    device::params::Image device_image_;                 // Device-compatible POD struct
    size_t batch_size_;                                  // Maximum samples per batch
    std::shared_ptr<cuda::Stream> stream_;               // Stream for operations

   public:
    // Constructor for buffer allocation
    Image(size_t width, size_t height, size_t num_samples_per_pixel, size_t batch_size,
          CUcontext ctx, std::shared_ptr<cuda::Stream> sample_buffer_stream,
          std::shared_ptr<cuda::Stream> averaged_pixels_stream)
        : variance_managed_(width * height, ctx, sample_buffer_stream,
                            cuda::AllocType::OnDeviceOnly),
          mean_managed_(width * height, ctx, sample_buffer_stream,
                        cuda::AllocType::OnDeviceOnly),
          sample_counts_managed_(width * height, ctx, sample_buffer_stream,
                                 cuda::AllocType::OnDeviceOnly),
          averaged_pixels_managed_(width * height, ctx, std::move(averaged_pixels_stream),
                                   cuda::AllocType::OnBoth),
          batch_size_(batch_size),
          stream_(sample_buffer_stream) {
        device_image_.variance_ = const_cast<float4*>(variance_managed_.device());
        device_image_.mean_ = const_cast<float4*>(mean_managed_.device());
        device_image_.sample_counts_ = const_cast<size_t*>(sample_counts_managed_.device());
        device_image_.width_ = width;
        device_image_.height_ = height;
        device_image_.image_size_ = width * height;
        device_image_.num_samples_per_pixel_ = num_samples_per_pixel;
        device_image_.batch_offset_ = 0;
        device_image_.batch_size_ = 0;  // Will be set per batch

        // Initialize Welford state and sample counts
        variance_managed_.memset_device(0);
        mean_managed_.memset_device(0);
        sample_counts_managed_.memset_device(0);
    }

    Image(Image&&) noexcept = default;
    Image& operator=(Image&&) noexcept = default;

    // Get device-compatible struct for launch params
    [[nodiscard]] const device::params::Image& device_image() const noexcept {
        return device_image_;
    }

    // Set batch parameters (called before each batch render)
    void set_batch_params(size_t batch_offset, size_t batch_size) noexcept {
        device_image_.batch_offset_ = batch_offset;
        device_image_.batch_size_ = batch_size;
    }

    // Utility methods
    [[nodiscard]] size_t width() const noexcept { return device_image_.width_; }
    [[nodiscard]] size_t height() const noexcept { return device_image_.height_; }
    [[nodiscard]] size_t num_samples_per_pixel() const noexcept {
        return device_image_.num_samples_per_pixel_;
    }
    [[nodiscard]] size_t batch_size() const noexcept { return batch_size_; }

    [[nodiscard]] size_t pixel_count() const noexcept { return device_image_.image_size_; }
    [[nodiscard]] size_t total_size() const noexcept {
        return device_image_.width_ * device_image_.height_ * device_image_.num_samples_per_pixel_;
    }

    [[nodiscard]] float aspect_ratio() const noexcept {
        return static_cast<float>(device_image_.width_) *
               common::math::rcp(static_cast<float>(device_image_.height_));
    }

    // Save final averaged image to EXR file
    [[nodiscard]] std::future<utils::Result<>> save(const std::filesystem::path& filename) {
        const auto& stream = averaged_pixels_managed_.get_context_param();

        spdlog::info("Saving render ({}x{}, {} spp)", device_image_.width_, device_image_.height_,
                     device_image_.num_samples_per_pixel_);

        // Copy Welford mean directly to output (already correctly averaged per-pixel)
        const size_t image_size = device_image_.image_size_;
        device::kernels::launch_normalize_accumulator_kernel(
            averaged_pixels_managed_.device(), mean_managed_.device(), image_size, 1.0f,
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
