#pragma once

#include "check.h"

#include <cuda_runtime.h>

#include <utility>

namespace thesis {

template <typename T>
class CudaUpload {
public:
    CudaUpload(const T& value) noexcept
    {
        CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&device_ptr_), sizeof(T)));
        CUDA_CHECK(cudaMemcpy(reinterpret_cast<void*>(device_ptr_), &value, sizeof(T), cudaMemcpyHostToDevice));
    }

    ~CudaUpload() noexcept
    {
        CUDA_CHECK(cudaFree(reinterpret_cast<void*>(device_ptr_)));
    }

    CudaUpload(const CudaUpload&) = delete;
    CudaUpload& operator=(const CudaUpload&) = delete;

    CudaUpload(CudaUpload&& other) noexcept
        : device_ptr_(std::exchange(other.device_ptr_, 0)) {}

    CudaUpload& operator=(CudaUpload&& other) noexcept
    {
        if (this != &other) {
            CUDA_CHECK(cudaFree(reinterpret_cast<void*>(device_ptr_)));
            device_ptr_ = std::exchange(other.device_ptr_, 0);
        }
        return *this;
    }

    operator CUdeviceptr() const noexcept { return device_ptr_; }

private:
    CUdeviceptr device_ptr_ = 0;
};

} // namespace thesis
