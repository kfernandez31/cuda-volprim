#pragma once

#include "thesis/host/utils/check.h"
#include <cuda_runtime.h>

#include <memory>

namespace thesis::cuda {

struct CudaDeleter {
    inline void operator()(void* ptr) const noexcept { CUDA_CHECK_NOEXCEPT(cudaFree(ptr)); }
};

template <typename T>
using UniqueDevicePtr = std::unique_ptr<T, CudaDeleter>;

template <typename T>
UniqueDevicePtr<T> makeDevicePtr(size_t count) {
    void* raw = nullptr;
    CUDA_CHECK(cudaMalloc(&raw, count * sizeof(T)));
    return UniqueDevicePtr<T>(static_cast<T*>(raw));
}

}  // namespace thesis::cuda
