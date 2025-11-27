#pragma once

#include "thesis/host/cuda/context.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/utils/check.h"

#include <cuda.h>
#include <cuda_runtime.h>

#include <cstdint>
#include <memory>
#include <span>

namespace thesis::host::cuda {

namespace detail {

struct CudaArrayDeleter {
    inline void operator()(cudaArray_t ptr) const noexcept {
        if (ptr) {
            CUDA_CHECK_NOEXCEPT(cudaFreeArray(ptr));
        }
    }
};

struct TextureObjectDeleter {
    cudaTextureObject_t handle_ = 0;

    inline void operator()(void*) const noexcept {
        if (handle_) {
            CUDA_CHECK_NOEXCEPT(cudaDestroyTextureObject(handle_));
        }
    }
};

}  // namespace detail

class CudaTexture {
   public:
    CudaTexture() = default;

    [[nodiscard]] static CudaTexture createRGBA(std::span<const float> data, size_t width,
                                                size_t height, size_t num_channels, CUcontext ctx,
                                                std::shared_ptr<Stream> stream) {
        Context::Guard g(ctx);

        if (num_channels != 4) {
            throw std::runtime_error("CudaTexture: Only 4-channel RGBA textures supported");
        }

        // Create channel descriptor for 32-bit float RGBA
        cudaChannelFormatDesc channel_desc =
            cudaCreateChannelDesc(32, 32, 32, 32,  // 32-bit R, G, B, A
                                  cudaChannelFormatKindFloat);

        // Allocate CUDA array (required for texture objects)
        cudaArray_t cu_array = nullptr;
        CUDA_CHECK(cudaMallocArray(&cu_array, &channel_desc, width, height));

        // Copy RGBA data to CUDA array
        const size_t pitch = width * num_channels * sizeof(float);
        CUDA_CHECK(cudaMemcpy2DToArray(cu_array, 0, 0,  // Offset
                                       data.data(),     // Source (already RGBA)
                                       pitch,           // Source pitch
                                       pitch,           // Width in bytes
                                       height, cudaMemcpyHostToDevice));

        // Create resource descriptor
        cudaResourceDesc res_desc = {};
        res_desc.resType = cudaResourceTypeArray;
        res_desc.res.array.array = cu_array;

        // Create texture descriptor
        cudaTextureDesc tex_desc = {};
        tex_desc.addressMode[0] = cudaAddressModeWrap;   // U wraps around (horizontal)
        tex_desc.addressMode[1] = cudaAddressModeClamp;  // V clamps at poles (vertical)
        tex_desc.filterMode = cudaFilterModeLinear;      // Bilinear interpolation
        tex_desc.readMode = cudaReadModeElementType;     // Return floats as-is
        tex_desc.normalizedCoords = 1;                   // Use [0,1] coordinates

        // Create texture object
        cudaTextureObject_t tex_obj = 0;
        CUDA_CHECK(cudaCreateTextureObject(&tex_obj, &res_desc, &tex_desc, nullptr));

        return CudaTexture(cu_array, tex_obj, width, height);
    }

    CudaTexture(CudaTexture&& other) noexcept
        : array_(std::move(other.array_)),
          texture_(std::move(other.texture_)),
          width_(std::exchange(other.width_, 0)),
          height_(std::exchange(other.height_, 0)) {}

    CudaTexture& operator=(CudaTexture&& other) noexcept {
        if (this != &other) {
            array_ = std::move(other.array_);
            texture_ = std::move(other.texture_);
            width_ = std::exchange(other.width_, 0);
            height_ = std::exchange(other.height_, 0);
        }
        return *this;
    }

    // Get texture object handle for device access // TODO: I don't understand why we'd need this,
    // i.e. why texture_ would be null. Don't I ensure it isn't?
    [[nodiscard]] cudaTextureObject_t get() const noexcept {
        return texture_ ? texture_.get_deleter().handle_ : 0;
    }

    [[nodiscard]] size_t width() const noexcept { return width_; }
    [[nodiscard]] size_t height() const noexcept { return height_; }

   private:
    CudaTexture(cudaArray_t array, cudaTextureObject_t tex_obj, size_t width, size_t height)
        : array_(array, detail::CudaArrayDeleter{}),
          texture_(reinterpret_cast<void*>(static_cast<uintptr_t>(1)),
                   detail::TextureObjectDeleter{tex_obj}),
          width_(width),
          height_(height) {}

    std::unique_ptr<cudaArray, detail::CudaArrayDeleter> array_;
    std::unique_ptr<void, detail::TextureObjectDeleter>
        texture_;  // Non-null dummy pointer (0x1), deleter holds actual handle // TODO: I don't
                   // understand this
    size_t width_;
    size_t height_;
};

}  // namespace thesis::host::cuda
