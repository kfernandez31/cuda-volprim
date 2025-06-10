#pragma once

#include "thesis/host/cuda/buffer_base.h"
#include "thesis/host/cuda/stream.h"

namespace thesis::host::cuda {

namespace detail {

struct PinnedHostDeleter {
    inline void operator()(void* ptr) const noexcept { CUDA_CHECK_NOEXCEPT(cudaFreeHost(ptr)); }
};

template <typename T>
using UniquePinnedHostPtr = std::unique_ptr<T, PinnedHostDeleter>;

template <typename T>
UniquePinnedHostPtr<T> makePinnedHostPtr(size_t count) {
    void* raw = nullptr;
    CUDA_CHECK(cudaHostAlloc(&raw, count * sizeof(T), cudaHostAllocDefault));
    return UniquePinnedHostPtr<T>(static_cast<T*>(raw));
}

}  // namespace detail

template <typename T>
class AsyncBuffer : public BufferBase<T> {
   private:
    detail::UniquePinnedHostPtr<T> host_ptr_;
    std::shared_ptr<Stream> stream_;

    AsyncBuffer(size_t count, CUcontext ctx, std::shared_ptr<Stream> stream, bool device_only)
        : BufferBase<T>(count, ctx),
          host_ptr_(device_only ? nullptr : detail::makePinnedHostPtr<T>(count)),
          stream_(std::move(stream)) {}

   public:
    AsyncBuffer() = default;

    [[nodiscard]] static AsyncBuffer onBoth(size_t count, CUcontext ctx,
                                            std::shared_ptr<Stream> stream) {
        return AsyncBuffer(count, ctx, std::move(stream), false);
    }

    [[nodiscard]] static AsyncBuffer onDeviceOnly(size_t count, CUcontext ctx,
                                                  std::shared_ptr<Stream> stream) {
        return AsyncBuffer(count, ctx, std::move(stream), true);
    }

    [[nodiscard]] static AsyncBuffer onBoth(std::span<const T> data, CUcontext ctx,
                                            std::shared_ptr<Stream> stream) {
        auto buf = onBoth(data.size(), ctx);
        std::memcpy(buf.host(), data.data(), data.size() * sizeof(T));
        buf.upload();
        return buf;
    }

    [[nodiscard]] static AsyncBuffer onDeviceOnly(std::span<const T> data, CUcontext ctx,
                                                  std::shared_ptr<Stream> stream) {
        auto buf = onDeviceOnly(data.size(), ctx, std::move(stream));
        buf.upload(data.data());
        return buf;
    }

    [[nodiscard]] T* host() noexcept override { return host_ptr_.get(); }
    [[nodiscard]] const T* host() const noexcept override { return host_ptr_.get(); }

    using BufferBase<T>::upload;
    using BufferBase<T>::download;

    void upload(const T* src) override {
        CUDA_CHECK(cudaMemcpyAsync(this->device(), src, this->size_in_bytes(),
                                   cudaMemcpyHostToDevice, stream_->get()));
    }

    void download(T* dst) override {
        CUDA_CHECK(cudaMemcpyAsync(dst, this->device(), this->size_in_bytes(),
                                   cudaMemcpyDeviceToHost, stream_->get()));
    }
};

}  // namespace thesis::host::cuda
