#pragma once

#include "thesis/utils/check.h"

#include <cuda_runtime.h>

#include <cstddef>
#include <utility>

// TODO(kacper): for real-time rendering and interop, opt for something like:
// C:\ProgramData\NVIDIA Corporation\OptiX SDK 9.0.0\SDK\sutil\CUDAOutputBuffer.h"

namespace thesis::cuda {

template <typename T>
class Buffer {
   public:
    explicit Buffer(size_t count) : count_(count), host_ptr_(new T[count_]), device_ptr_(nullptr) {
        CUDA_CHECK(cudaMalloc(&device_ptr_, count_ * sizeof(T)));
    }

    ~Buffer() noexcept {
        delete[] host_ptr_;
        CUDA_CHECK_NOEXCEPT(cudaFree(device_ptr_));
    }

    // Disable copy
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // Enable move ctor
    Buffer(Buffer&& other) noexcept
        : count_(std::exchange(other.count_, 0)),
          host_ptr_(std::exchange(other.host_ptr_, nullptr)),
          device_ptr_(std::exchange(other.device_ptr_, nullptr)) {}

    // Enable move assignment
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            delete[] host_ptr_;
            CUDA_CHECK_NOEXCEPT(cudaFree(device_ptr_));

            host_ptr_ = std::exchange(other.host_ptr_, nullptr);
            device_ptr_ = std::exchange(other.device_ptr_, nullptr);
            count_ = std::exchange(other.count_, 0);
        }
        return *this;
    }

    void upload() {
        CUDA_CHECK(cudaMemcpy(device_ptr_, host_ptr_, count_ * sizeof(T), cudaMemcpyHostToDevice));
    }

    void download() {
        CUDA_CHECK(cudaMemcpy(host_ptr_, device_ptr_, count_ * sizeof(T), cudaMemcpyDeviceToHost));
    }

    [[nodiscard]] T* host() noexcept { return host_ptr_; }
    [[nodiscard]] const T* host() const noexcept { return host_ptr_; }

    [[nodiscard]] T* device() noexcept { return device_ptr_; }
    [[nodiscard]] const T* device() const noexcept { return device_ptr_; }

    [[nodiscard]] size_t size() const { return count_; }

   private:
    size_t count_ = 0;
    T* host_ptr_ = nullptr;
    T* device_ptr_ = nullptr;
};

}  // namespace thesis::cuda
