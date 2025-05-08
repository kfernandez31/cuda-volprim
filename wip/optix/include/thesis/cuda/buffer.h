#pragma once

#include "thesis/cuda/device_ptr.h"
#include "thesis/utils/check.h"

#include <cuda_runtime.h>

#include <cstddef>
#include <memory>

// TODO(kacper): for real-time rendering and interop, opt for something like:
// C:\ProgramData\NVIDIA Corporation\OptiX SDK 9.0.0\SDK\sutil\CUDAOutputBuffer.h"

namespace thesis::cuda {

template <typename T>
class Buffer {
   public:
    Buffer() = default;

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

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&&) noexcept = default;
    Buffer& operator=(Buffer&&) noexcept = default;

    void upload() {
        CUDA_CHECK(cudaMemcpy(device_ptr_.get(), host_ptr_.get(), count_ * sizeof(T),
                              cudaMemcpyHostToDevice));
    }

    void download() {
        CUDA_CHECK(cudaMemcpy(host_ptr_.get(), device_ptr_.get(), count_ * sizeof(T),
                              cudaMemcpyDeviceToHost));
    }

    [[nodiscard]] T* host() noexcept { return host_ptr_.get(); }
    [[nodiscard]] const T* host() const noexcept { return host_ptr_.get(); }

    [[nodiscard]] T* device() noexcept { return device_ptr_.get(); }
    [[nodiscard]] const T* device() const noexcept { return device_ptr_.get(); }

    [[nodiscard]] size_t size() const noexcept { return count_; }
    [[nodiscard]] bool empty() const noexcept { return count_ == 0; }

   private:
    size_t count_;
    std::unique_ptr<T[]> host_ptr_;
    UniqueDevicePtr<T> device_ptr_;
};

}  // namespace thesis::cuda
