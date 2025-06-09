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

    Buffer(size_t cnt, CUcontext ctx, bool device_only = false)
        : count_(cnt),
          host_ptr_(device_only ? nullptr : std::make_unique<T[]>(cnt)),
          device_ptr_(makeDevicePtr<T>(cnt, ctx)) {}

   public:
    Buffer() = default;

    Buffer(Buffer&&) noexcept = default;
    Buffer& operator=(Buffer&&) noexcept = default;

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    static Buffer onBoth(size_t cap, CUcontext ctx) { return Buffer(cap, ctx); }

    static Buffer onBoth(std::span<const T> data, CUcontext ctx) {
        auto buf = onBoth(data.size(), ctx);
        buf.upload(data);
        return buf;
    }

    static Buffer onDeviceOnly(size_t cap, CUcontext ctx) { return Buffer(cap, ctx, true); }

    static Buffer onDeviceOnly(std::span<const T> data, CUcontext ctx) {
        auto buf = onDeviceOnly(data.size(), ctx);
        buf.upload(data.data());
        return buf;
    }

    void upload(const T* src) {
        CUDA_CHECK(cudaMemcpy(device(), src, size_in_bytes(), cudaMemcpyHostToDevice));
    }

    void upload() { upload(host()); }

    void download(T* dst) {
        CUDA_CHECK(cudaMemcpy(dst, device(), size_in_bytes(), cudaMemcpyDeviceToHost));
    }

    void download() { download(host()); }

    [[nodiscard]] size_t size() const noexcept { return count_; }
    [[nodiscard]] size_t size_in_bytes() const noexcept { return count_ * sizeof(T); }

    [[nodiscard]] T* host() noexcept { return host_ptr_.get(); }
    [[nodiscard]] const T* host() const noexcept { return host_ptr_.get(); }

    [[nodiscard]] T* device() noexcept { return device_ptr_.get(); }
    [[nodiscard]] const T* device() const noexcept { return device_ptr_.get(); }
    [[nodiscard]] CUdeviceptr cu_device_ptr() const noexcept {
        return reinterpret_cast<CUdeviceptr>(device());
    }

    [[nodiscard]] std::span<T> host_view() noexcept { return {host(), count_}; }
    [[nodiscard]] std::span<const T> host_view() const noexcept { return {host(), count_}; }

    [[nodiscard]] T* begin() noexcept { return host(); }
    [[nodiscard]] T* end() noexcept { return host() + count_; }

    [[nodiscard]] const T* begin() const noexcept { return host(); }
    [[nodiscard]] const T* end() const noexcept { return host() + count_; }

    [[nodiscard]] T& operator[](size_t index) noexcept { return host()[index]; }
    [[nodiscard]] const T& operator[](size_t index) const noexcept { return host()[index]; }

};

}  // namespace thesis::host::cuda
