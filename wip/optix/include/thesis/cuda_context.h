#pragma once

#ifdef __cplusplus

#include <cuda.h>

namespace thesis {

class CudaContextHandle {
public:
    explicit CudaContextHandle(int device_ordinal = 0);
    ~CudaContextHandle();

    // Disable copy
    CudaContextHandle(const CudaContextHandle&) = delete;
    CudaContextHandle& operator=(const CudaContextHandle&) = delete;

    // Enable move
    CudaContextHandle(CudaContextHandle&& other) noexcept;
    CudaContextHandle& operator=(CudaContextHandle&& other) noexcept;

    [[nodiscard]] const CUcontext& get() const noexcept { return context_; }
    [[nodiscard]]       CUcontext& get()       noexcept { return context_; }

    [[nodiscard]] CUdevice device() const noexcept { return device_; }

private:
    CUcontext context_ = nullptr;
    CUdevice device_ = -1;
};

} // namespace thesis

#endif // __cplusplus
