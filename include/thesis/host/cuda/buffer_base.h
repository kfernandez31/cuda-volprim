#pragma once

#include "thesis/host/cuda/context.h"
#include "thesis/host/utils/check.h"

#include <cuda_runtime.h>

#include <cstddef>
#include <memory>
#include <span>

namespace thesis::host::cuda {

namespace detail {

struct DeviceDeleter {
    inline void operator()(void* ptr) const noexcept { CUDA_CHECK_NOEXCEPT(cudaFree(ptr)); }
};

template <typename T>
using UniqueDevicePtr = std::unique_ptr<T, DeviceDeleter>;

template <typename T>
UniqueDevicePtr<T> makeDevicePtr(size_t count, CUcontext ctx) {
    Context::Guard guard(ctx);

    void* raw = nullptr;
    CUDA_CHECK(cudaMalloc(&raw, count * sizeof(T)));
    return UniqueDevicePtr<T>(static_cast<T*>(raw));
}

} // namespace detail

template <typename T>
class BufferBase {
protected:
    size_t count_ = 0;
    detail::UniqueDevicePtr<T> device_ptr_ = nullptr;

    BufferBase(size_t count, CUcontext ctx) : count_(count), device_ptr_(detail::makeDevicePtr<T>(count, ctx)) {}
    
public:
    BufferBase() = default;
    BufferBase(const BufferBase&) = delete;
    BufferBase& operator=(const BufferBase&) = delete;

    BufferBase(BufferBase&&) noexcept = default;
    BufferBase& operator=(BufferBase&&) noexcept = default;

    [[nodiscard]] size_t size() const noexcept { return count_; }
    [[nodiscard]] size_t size_in_bytes() const noexcept { return size() * sizeof(T); }

    [[nodiscard]] virtual T* host() noexcept = 0;
    [[nodiscard]] virtual const T* host() const noexcept = 0;

    [[nodiscard]] T* device() noexcept { return device_ptr_.get(); }
    [[nodiscard]] const T* device() const noexcept { return device_ptr_.get(); }

    virtual void upload(const T* src) = 0;
    virtual void download(T* dst) = 0;

    void upload() { upload(host()); }
    void download() { download(host()); }

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

} // namespace thesis::host::cuda
