#pragma once

#include <cuda.h>

namespace thesis {

class CudaContextHandle {
public:
    explicit CudaContextHandle(int device_ordinal = 0) noexcept; // TODO: why `explicit`?
    ~CudaContextHandle() noexcept;

    // Disable copy
    CudaContextHandle(const CudaContextHandle&) = delete;
    CudaContextHandle& operator=(const CudaContextHandle&) = delete;

    // Enable move
    CudaContextHandle(CudaContextHandle&& other) noexcept;
    CudaContextHandle& operator=(CudaContextHandle&& other) noexcept;

    operator CUcontext() const noexcept { return context_; }
    CUdevice device() const noexcept { return device_; }

private:
    CUcontext context_ = nullptr;
    CUdevice device_ = -1;
};

} // namespace thesis