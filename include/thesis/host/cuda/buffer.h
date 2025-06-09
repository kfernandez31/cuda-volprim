#pragma once

#include "thesis/host/cuda/context.h"
#include "thesis/host/utils/check.h"

#include <cuda_runtime.h>

#include <cstddef>
#include <memory>
#include <span>

namespace thesis::host::cuda {

struct CudaDeleter {
    inline void operator()(void* ptr) const noexcept { CUDA_CHECK_NOEXCEPT(cudaFree(ptr)); }
};

template <typename T>
using UniqueDevicePtr = std::unique_ptr<T, CudaDeleter>;

template <typename T>
UniqueDevicePtr<T> makeDevicePtr(size_t count, CUcontext ctx) {
    Context::Guard guard(ctx);

    void* raw = nullptr;
    CUDA_CHECK(cudaMalloc(&raw, count * sizeof(T)));
    return UniqueDevicePtr<T>(static_cast<T*>(raw));
}

template <typename T>
class Buffer {
   private:
    size_t count_ = 0;
    std::unique_ptr<T[]> host_ptr_ = nullptr;
    UniqueDevicePtr<T> device_ptr_ = nullptr;

    Buffer(size_t cap, CUcontext ctx, bool device_only = false)
        : count_(cap),
          host_ptr_(device_only ? nullptr : std::make_unique<T[]>(cap)),
          device_ptr_(makeDevicePtr<T>(cap, ctx)) {}

   public:
    Buffer() = default;

    Buffer(Buffer&&) noexcept = default;
    Buffer& operator=(Buffer&&) noexcept = default;

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    static Buffer onBoth(size_t cap, CUcontext ctx) {
        return Buffer(cap, ctx);
    }

    static Buffer onBoth(std::span<const T> data, CUcontext ctx) {
        auto buf = onBoth(data.size(), ctx);
        buf.push_back(data);
        return buf;
    }

    static Buffer onDeviceOnly(size_t cap, CUcontext ctx) { 
        return Buffer(cap, ctx, true);
    }

    static Buffer onDeviceOnly(std::span<const T> data, CUcontext ctx) {
        auto buf = onDeviceOnly(data.size(), ctx);
        CUDA_CHECK(cudaMemcpy(buf.device(), data.data(), data.size() * sizeof(T),
                              cudaMemcpyHostToDevice));
        return buf;
    }

    T* upload() {
        CUDA_CHECK(cudaMemcpy(device_ptr_.get(), host_ptr_.get(), count_ * sizeof(T),
                              cudaMemcpyHostToDevice));
        return device();
    }

    T* download() {
        CUDA_CHECK(cudaMemcpy(host_ptr_.get(), device_ptr_.get(), count_ * sizeof(T),
                              cudaMemcpyDeviceToHost));
        return host();
    }

    [[nodiscard]] std::span<const T> host_view() const noexcept {
        return std::span<const T>(host_ptr_.get(), count_);
    }

    [[nodiscard]] std::span<T> host_view() noexcept {
        return std::span<T>(host_ptr_.get(), count_);
    }

    [[nodiscard]] T* host() noexcept { return host_ptr_.get(); }
    [[nodiscard]] const T* host() const noexcept { return host_ptr_.get(); }

    [[nodiscard]] T* device() noexcept { return device_ptr_.get(); }
    [[nodiscard]] const T* device() const noexcept { return device_ptr_.get(); }

    [[nodiscard]] size_t size() const noexcept { return count_; }
    [[nodiscard]] bool empty() const noexcept { return count_ == 0; }

    [[nodiscard]] T* begin() noexcept { return host_ptr_.get(); }
    [[nodiscard]] T* end() noexcept { return host_ptr_.get() + count_; }

    [[nodiscard]] const T* begin() const noexcept { return host_ptr_.get(); }
    [[nodiscard]] const T* end() const noexcept { return host_ptr_.get() + count_; }

    [[nodiscard]] const T* cbegin() const noexcept { return host_ptr_.get(); }
    [[nodiscard]] const T* cend() const noexcept { return host_ptr_.get() + count_; }
};

}  // namespace thesis::host::cuda
