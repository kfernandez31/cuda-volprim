#pragma once

#include "thesis/host/cuda/async_buffer.h"
#include "thesis/host/cuda/context.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/utils/check.h"

#include <optix_stubs.h>
#include <vector_types.h>

#include <cstddef>
#include <memory>
#include <spdlog/spdlog.h>
#include <utility>

namespace thesis::host::optix {

class Denoiser {
    OptixDenoiser handle_ = nullptr;
    cuda::AsyncBuffer<uint8_t> state_;
    cuda::AsyncBuffer<uint8_t> scratch_;
    std::shared_ptr<cuda::Stream> stream_;
    uint32_t width_ = 0;
    uint32_t height_ = 0;

   public:
    Denoiser(OptixDeviceContext optix_ctx, uint32_t width, uint32_t height, CUcontext cu_ctx,
             std::shared_ptr<cuda::Stream> stream)
        : stream_(std::move(stream)),
          width_(width),
          height_(height) {
        OptixDenoiserOptions options{};
        options.guideAlbedo = 0;
        options.guideNormal = 0;

        OPTIX_CHECK(
            optixDenoiserCreate(optix_ctx, OPTIX_DENOISER_MODEL_KIND_HDR, &options, &handle_));

        OptixDenoiserSizes sizes{};
        OPTIX_CHECK(optixDenoiserComputeMemoryResources(handle_, width, height, &sizes));

        state_ = cuda::AsyncBuffer<uint8_t>(sizes.stateSizeInBytes, cu_ctx, stream_,
                                            cuda::AllocType::OnDeviceOnly);
        scratch_ = cuda::AsyncBuffer<uint8_t>(sizes.withoutOverlapScratchSizeInBytes, cu_ctx,
                                              stream_, cuda::AllocType::OnDeviceOnly);

        OPTIX_CHECK(optixDenoiserSetup(handle_, stream_->get(), width, height,
                                       state_.cu_device_ptr(), state_.size_bytes(),
                                       scratch_.cu_device_ptr(), scratch_.size_bytes()));

        stream_->synchronize();
        spdlog::info("OptiX HDR denoiser initialized ({}x{})", width, height);
    }

    ~Denoiser() { reset(); }

    Denoiser(Denoiser&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)),
          state_(std::move(other.state_)),
          scratch_(std::move(other.scratch_)),
          stream_(std::move(other.stream_)),
          width_(std::exchange(other.width_, 0)),
          height_(std::exchange(other.height_, 0)) {}

    Denoiser& operator=(Denoiser&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, nullptr);
            state_ = std::move(other.state_);
            scratch_ = std::move(other.scratch_);
            stream_ = std::move(other.stream_);
            width_ = std::exchange(other.width_, 0);
            height_ = std::exchange(other.height_, 0);
        }
        return *this;
    }

    // Denoise in-place on a device float4 buffer
    void invoke(float4* device_pixels) const {
        const OptixDenoiserGuideLayer guide{};

        OptixImage2D image{};
        image.data = reinterpret_cast<CUdeviceptr>(device_pixels);
        image.width = width_;
        image.height = height_;
        image.rowStrideInBytes = width_ * sizeof(float4);
        image.pixelStrideInBytes = sizeof(float4);
        image.format = OPTIX_PIXEL_FORMAT_FLOAT4;

        OptixDenoiserLayer layer{};
        layer.input = image;
        layer.output = image;  // in-place

        const OptixDenoiserParams params{};
        // TODO: params.denoiseAlpha = OPTIX_DENOISER_ALPHA_MODE_COPY; // API changed in newer OptiX

        OPTIX_CHECK(optixDenoiserInvoke(handle_, stream_->get(), &params, state_.cu_device_ptr(),
                                        state_.size_bytes(), &guide, &layer, 1, 0, 0,
                                        scratch_.cu_device_ptr(), scratch_.size_bytes()));

        stream_->synchronize();
        spdlog::info("Denoising complete");
    }

   private:
    void reset() noexcept {
        if (handle_) {
            OPTIX_CHECK_NOEXCEPT(optixDenoiserDestroy(handle_));
            handle_ = nullptr;
        }
    }
};

}  // namespace thesis::host::optix
