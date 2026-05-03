#pragma once

#include "core/constants.cuh"
#include "kernels/normalize_accumulator.h"
#include "thesis/common/utils/math.h"
#include "thesis/device/params/image.h"
#include "thesis/host/cuda/async_buffer.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/optix/denoiser.h"
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
    cuda::AsyncBuffer<float4> variance_managed_;         // Welford M2 (only allocated when ENABLE_ADAPTIVE_SAMPLING)
    cuda::AsyncBuffer<float4> mean_managed_;             // Running mean for Welford's algorithm
    cuda::AsyncBuffer<uint16_t> sample_counts_managed_;  // Per-pixel sample counts (max 65535 spp)
    cuda::AsyncBuffer<float4> averaged_pixels_managed_;  // Final output (RGBA, W unused)
    cuda::AsyncBuffer<float4> albedo_aov_managed_;       // Denoiser albedo guide (only when denoising)
    cuda::AsyncBuffer<float4> normal_aov_managed_;       // Denoiser normal guide (only when denoising)
    device::params::Image device_image_;                 // Device-compatible POD struct
    size_t batch_size_;                                  // Maximum samples per batch
    std::shared_ptr<cuda::Stream> stream_;               // Stream for operations

   public:
    // Constructor for buffer allocation. `enable_aovs` allocates the denoiser
    // guide-layer buffers; pass false when not denoising to save 2·W·H·16B of
    // device memory plus a per-sample write each. Variance buffer is gated on
    // device::consts::ENABLE_ADAPTIVE_SAMPLING (compile-time, in constants.cuh).
    Image(size_t width, size_t height, size_t num_samples_per_pixel, size_t batch_size,
          bool enable_aovs, CUcontext ctx,
          std::shared_ptr<cuda::Stream> sample_buffer_stream,
          std::shared_ptr<cuda::Stream> averaged_pixels_stream)
        : mean_managed_(width * height, ctx, sample_buffer_stream, cuda::AllocType::OnDeviceOnly),
          sample_counts_managed_(width * height, ctx, sample_buffer_stream,
                                 cuda::AllocType::OnDeviceOnly),
          averaged_pixels_managed_(width * height, ctx, std::move(averaged_pixels_stream),
                                   cuda::AllocType::OnBoth),
          batch_size_(batch_size),
          stream_(sample_buffer_stream) {
        if constexpr (device::consts::ENABLE_ADAPTIVE_SAMPLING) {
            variance_managed_ = cuda::AsyncBuffer<float4>(width * height, ctx, sample_buffer_stream,
                                                          cuda::AllocType::OnDeviceOnly);
            variance_managed_.memset_device(0);
            device_image_.variance_ = const_cast<float4*>(variance_managed_.device());
        } else {
            device_image_.variance_ = nullptr;
        }

        if (enable_aovs) {
            albedo_aov_managed_ = cuda::AsyncBuffer<float4>(width * height, ctx,
                                                            sample_buffer_stream,
                                                            cuda::AllocType::OnDeviceOnly);
            normal_aov_managed_ = cuda::AsyncBuffer<float4>(width * height, ctx,
                                                            sample_buffer_stream,
                                                            cuda::AllocType::OnDeviceOnly);
            albedo_aov_managed_.memset_device(0);
            normal_aov_managed_.memset_device(0);
            device_image_.albedo_aov_ = const_cast<float4*>(albedo_aov_managed_.device());
            device_image_.normal_aov_ = const_cast<float4*>(normal_aov_managed_.device());
        } else {
            device_image_.albedo_aov_ = nullptr;
            device_image_.normal_aov_ = nullptr;
        }

        device_image_.mean_ = const_cast<float4*>(mean_managed_.device());
        device_image_.sample_counts_ = const_cast<uint16_t*>(sample_counts_managed_.device());
        device_image_.width_ = static_cast<uint32_t>(width);
        device_image_.height_ = static_cast<uint32_t>(height);
        device_image_.num_samples_per_pixel_ = static_cast<uint32_t>(num_samples_per_pixel);
        device_image_.batch_offset_ = 0;
        device_image_.batch_size_ = 0;  // Will be set per batch

        mean_managed_.memset_device(0);
        sample_counts_managed_.memset_device(0);
    }

    // Device pointers for the AOV buffers (read-only — caller must not free).
    [[nodiscard]] const float4* albedo_aov_device() const noexcept {
        return albedo_aov_managed_.device();
    }
    [[nodiscard]] const float4* normal_aov_device() const noexcept {
        return normal_aov_managed_.device();
    }

    Image(Image&&) noexcept = default;
    Image& operator=(Image&&) noexcept = default;

    // Get device-compatible struct for launch params
    [[nodiscard]] const device::params::Image& device_image() const noexcept {
        return device_image_;
    }

    // Set batch parameters (called before each batch render)
    void set_batch_params(size_t batch_offset, size_t batch_size) noexcept {
        device_image_.batch_offset_ = static_cast<uint32_t>(batch_offset);
        device_image_.batch_size_ = static_cast<uint32_t>(batch_size);
    }

    // Utility methods
    [[nodiscard]] size_t width() const noexcept { return device_image_.width_; }
    [[nodiscard]] size_t height() const noexcept { return device_image_.height_; }
    [[nodiscard]] size_t num_samples_per_pixel() const noexcept {
        return device_image_.num_samples_per_pixel_;
    }
    [[nodiscard]] size_t batch_size() const noexcept { return batch_size_; }

    [[nodiscard]] size_t pixel_count() const noexcept {
        return static_cast<size_t>(device_image_.width_) * device_image_.height_;
    }
    [[nodiscard]] size_t total_size() const noexcept {
        return device_image_.width_ * device_image_.height_ * device_image_.num_samples_per_pixel_;
    }

    [[nodiscard]] float aspect_ratio() const noexcept {
        return static_cast<float>(device_image_.width_) *
               common::math::rcp(static_cast<float>(device_image_.height_));
    }

    // Save final averaged image to EXR file
    [[nodiscard]] std::future<utils::Result<>> save(const std::filesystem::path& filename) {
        normalize();

        averaged_pixels_managed_.download();
        averaged_pixels_managed_.get_context_param()->synchronize();

        spdlog::info("Saving to '{}'", filename.string());
        // Move buffer ownership to async task (zero-copy)
        return utils::io::async::saveExr(std::move(averaged_pixels_managed_), device_image_.width_,
                                         device_image_.height_, filename);
    }

    // Denoise and save both raw and denoised images
    // Raw image saved to filename, denoised saved to {stem}_denoised{ext}
    [[nodiscard]] std::pair<std::future<utils::Result<>>, std::future<utils::Result<>>>
    denoise_and_save(const optix::Denoiser& denoiser, const std::filesystem::path& filename,
                     CUcontext cu_ctx) {
        normalize();

        // Download raw pixels to host
        averaged_pixels_managed_.download();
        averaged_pixels_managed_.get_context_param()->synchronize();

        // Save raw image (copy host data, don't move — we still need the device buffer)
        auto raw_path = filename;
        auto raw_future = utils::io::async::saveExr(
            cuda::AsyncBuffer<float4>(averaged_pixels_managed_.host_view(), cu_ctx,
                                      averaged_pixels_managed_.get_context_param()),
            device_image_.width_, device_image_.height_, raw_path);

        // Denoise in-place on device, supplying albedo + normal AOVs as guide layers.
        denoiser.invoke(averaged_pixels_managed_.device(), albedo_aov_managed_.device(),
                        normal_aov_managed_.device());

        // Download denoised pixels
        averaged_pixels_managed_.download();
        averaged_pixels_managed_.get_context_param()->synchronize();

        // Save denoised image
        auto denoised_path = filename.parent_path() / (filename.stem().string() + "_denoised" +
                                                       filename.extension().string());
        spdlog::info("Saving denoised to '{}'", denoised_path.string());
        auto denoised_future =
            utils::io::async::saveExr(std::move(averaged_pixels_managed_), device_image_.width_,
                                      device_image_.height_, denoised_path);

        return {std::move(raw_future), std::move(denoised_future)};
    }

   private:
    void normalize() {
        const auto& stream = averaged_pixels_managed_.get_context_param();

        spdlog::info("Saving render ({}x{}, {} spp)", device_image_.width_, device_image_.height_,
                     device_image_.num_samples_per_pixel_);

        // Copy Welford mean directly to output (already correctly averaged per-pixel)
        device::kernels::launch_normalize_accumulator_kernel(averaged_pixels_managed_.device(),
                                                             mean_managed_.device(), pixel_count(),
                                                             1.0f, stream->get());
    }
};

}  // namespace params
}  // namespace host
}  // namespace thesis
