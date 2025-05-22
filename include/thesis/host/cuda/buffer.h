#pragma once

#include "thesis/cuda/device_ptr.h"
#include "thesis/utils/check.h"

#include <cuda_runtime.h>

#include <cstddef>
#include <span>
#include <memory>

// TODO(kacper): for real-time rendering and interop, opt for something like:
// C:\ProgramData\NVIDIA Corporation\OptiX SDK 9.0.0\SDK\sutil\CUDAOutputBuffer.h"

namespace thesis::cuda {

template <typename T>
class Buffer {
   private:
    size_t count_ = 0;
    std::unique_ptr<T[]> host_ptr_ = nullptr;
    UniqueDevicePtr<T> device_ptr_ = nullptr;
   public:
    Buffer() = default;

    Buffer(Buffer&&) noexcept = default;
    Buffer& operator=(Buffer&&) noexcept = default;

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    explicit Buffer(size_t count)
        : count_(count),
          host_ptr_(std::make_unique<T[]>(count)),
          device_ptr_(makeDevicePtr<T>(count)) {}

    Buffer(const T* data, size_t count) : Buffer(count) {
        CUDA_CHECK(cudaMemcpy(device_ptr_.get(), data, sizeof(T) * count, cudaMemcpyHostToDevice));
    }

    static Buffer onDeviceOnly(size_t count) {
        Buffer buf;
        buf.count_ = count;
        buf.device_ptr_ = makeDevicePtr<T>(count);
        return buf;
    }

    static Buffer onDeviceOnly(const T* data, size_t count) {
        Buffer buf = onDeviceOnly(count);
        CUDA_CHECK(
            cudaMemcpy(buf.device_ptr_.get(), data, sizeof(T) * count, cudaMemcpyHostToDevice));
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

}  // namespace thesis::cuda
