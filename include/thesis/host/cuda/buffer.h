#pragma once

#include "thesis/host/cuda/buffer_base.h"

namespace thesis::host::cuda {

template <typename T>
class Buffer : public BufferBase<T> {
   private:
    std::unique_ptr<T[]> host_ptr_;

    Buffer(size_t count, CUcontext ctx, bool device_only)
        : BufferBase<T>(count, ctx),
          host_ptr_(device_only ? nullptr : std::make_unique<T[]>(count)) {}

   public:
    Buffer() = default;

    [[nodiscard]] static Buffer onBoth(size_t count, CUcontext ctx) {
        return Buffer(count, ctx, false);
    }

    [[nodiscard]] static Buffer onDeviceOnly(size_t count, CUcontext ctx) {
        return Buffer(count, ctx, true);
    }

    [[nodiscard]] static Buffer onBoth(std::span<const T> data, CUcontext ctx) {
        auto buf = onBoth(data.size(), ctx);
        std::memcpy(buf.host(), data.data(), data.size() * sizeof(T));
        buf.upload();
        return buf;
    }

    [[nodiscard]] static Buffer onDeviceOnly(std::span<const T> data, CUcontext ctx) {
        auto buf = onDeviceOnly(data.size(), ctx);
        buf.upload(data.data());
        return buf;
    }

    [[nodiscard]] T* host() noexcept override { return host_ptr_.get(); }
    [[nodiscard]] const T* host() const noexcept override { return host_ptr_.get(); }

    using BufferBase<T>::upload;
    using BufferBase<T>::download;

    void upload(const T* src) {
        CUDA_CHECK(cudaMemcpy(this->device(), src, this->size_in_bytes(), cudaMemcpyHostToDevice));
    }

    void download(T* dst) {
        CUDA_CHECK(cudaMemcpy(dst, this->device(), this->size_in_bytes(), cudaMemcpyDeviceToHost));
    }
};

}  // namespace thesis::host::cuda
