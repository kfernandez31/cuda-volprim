#pragma once

#ifdef __cplusplus

#include "thesis/check.h"

#include <cuda_runtime.h>

#include <utility>
#include <cstddef> 

namespace thesis {

// TODO(kacper): for real-time rendering and interop, opt for something like:
// C:\ProgramData\NVIDIA Corporation\OptiX SDK 9.0.0\SDK\sutil\CUDAOutputBuffer.h" 

template <typename T>
class CudaBuffer {
public:
    explicit CudaBuffer(size_t count)
        : count_(count), host_ptr_(new T[count_]), device_ptr_(nullptr)
    {
        CUDA_CHECK(cudaMalloc(&device_ptr_, count_ * sizeof(T)));
    }

    ~CudaBuffer() noexcept
    {
        delete[] host_ptr_;
        CUDA_CHECK_NOEXCEPT(cudaFree(device_ptr_));
    }

    // Disable copy
    CudaBuffer(const CudaBuffer&) = delete;
    CudaBuffer& operator=(const CudaBuffer&) = delete;

    // Enable move ctor
    CudaBuffer(CudaBuffer&& other) noexcept
        : count_(std::exchange(other.count_, 0))
        , host_ptr_(std::exchange(other.host_ptr_, nullptr))
        , device_ptr_(std::exchange(other.device_ptr_, nullptr)) {}

    // Enable move assignment
    CudaBuffer& operator=(CudaBuffer&& other) noexcept
    {
        if (this != &other) {
            delete[] host_ptr_;
            CUDA_CHECK_NOEXCEPT(cudaFree(device_ptr_));

            host_ptr_ = std::exchange(other.host_ptr_, nullptr);
            device_ptr_ = std::exchange(other.device_ptr_, nullptr);
            count_ = std::exchange(other.count_, 0);
        }
        return *this;
    }

    void upload()
    {
        CUDA_CHECK(cudaMemcpy(device_ptr_, host_ptr_, count_ * sizeof(T), cudaMemcpyHostToDevice));
    }

    void download()
    {
        CUDA_CHECK(cudaMemcpy(host_ptr_, device_ptr_, count_ * sizeof(T), cudaMemcpyDeviceToHost));
    }

    [[nodiscard]]       T* host()       noexcept { return host_ptr_; }
    [[nodiscard]] const T* host() const noexcept { return host_ptr_; }

    [[nodiscard]]       T* device()       noexcept { return device_ptr_; }
    [[nodiscard]] const T* device() const noexcept { return device_ptr_; }

    [[nodiscard]] size_t size() const { return count_; }
private:
    size_t count_ = 0;
    T* host_ptr_ = nullptr;
    T* device_ptr_ = nullptr;
};

} // namespace thesis


#endif // __cplusplus
