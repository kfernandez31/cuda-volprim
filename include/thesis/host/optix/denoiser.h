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
        // Always create with guide layers enabled — the runtime cost is small and
        // skipping guides at invoke-time is supported (pass null pointers).
        options.guideAlbedo = 1;
        options.guideNormal = 1;

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

    // Denoise in-place on a device float4 buffer.
    // albedo_aov / normal_aov are optional float4 guide layers (per-pixel running means
    // of first-scatter albedo and -ray.direction). Pass null to denoise without guides.
    void invoke(float4* device_pixels, const float4* albedo_aov = nullptr,
                const float4* normal_aov = nullptr) const {
        const auto make_image = [&](CUdeviceptr ptr) {
            OptixImage2D img{};
            img.data = ptr;
            img.width = width_;
            img.height = height_;
            img.rowStrideInBytes = width_ * sizeof(float4);
            img.pixelStrideInBytes = sizeof(float4);
            img.format = OPTIX_PIXEL_FORMAT_FLOAT4;
            return img;
        };

        OptixDenoiserGuideLayer guide{};
        if (albedo_aov) {
            guide.albedo = make_image(reinterpret_cast<CUdeviceptr>(albedo_aov));
        }
        if (normal_aov) {
            guide.normal = make_image(reinterpret_cast<CUdeviceptr>(normal_aov));
        }

        OptixDenoiserLayer layer{};
        layer.input = make_image(reinterpret_cast<CUdeviceptr>(device_pixels));
        layer.output = layer.input;  // in-place

        OptixDenoiserParams params{};
        // TODO: params.denoiseAlpha = OPTIX_DENOISER_ALPHA_MODE_COPY; // API changed in newer OptiX

        OPTIX_CHECK(optixDenoiserInvoke(handle_, stream_->get(), &params, state_.cu_device_ptr(),
                                        state_.size_bytes(), &guide, &layer, 1, 0, 0,
                                        scratch_.cu_device_ptr(), scratch_.size_bytes()));

        stream_->synchronize();
        spdlog::info("Denoising complete (guides: albedo={}, normal={})",
                     albedo_aov ? "on" : "off", normal_aov ? "on" : "off");
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
