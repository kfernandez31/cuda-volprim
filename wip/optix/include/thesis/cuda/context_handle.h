#pragma once

#ifdef __cplusplus

#include <cuda.h>

#include <utility>

namespace thesis::cuda {

class ContextHandle {
public:
    explicit ContextHandle(int device_ordinal = 0) {
        CU_CHECK(cuInit(0));
        CU_CHECK(cuDeviceGet(&device_, device_ordinal));
        CU_CHECK(cuCtxCreate(&context_, 0, device_));
    }

    ~ContextHandle()
    {
        if (context_ != nullptr) {
            CU_CHECK(cuCtxDestroy(context_));
        }
    }

    // Disable copy
    ContextHandle(const ContextHandle&) = delete;
    ContextHandle& operator=(const ContextHandle&) = delete;

    // Enable move ctor
    ContextHandle(ContextHandle&& other) noexcept
        : context_(std::exchange(other.context_, nullptr))
        , device_(std::exchange(other.device_, -1)) {}
    
    // Enable move assignment
    ContextHandle& operator=(ContextHandle&& other) noexcept 
    {
        if (this != &other) {
            if (context_ != nullptr) {
                CU_CHECK_NOEXCEPT(cuCtxDestroy(context_));
            }
            context_ = std::exchange(other.context_, nullptr);
            device_ = std::exchange(other.device_, -1);
        }
        return *this;
    }

    [[nodiscard]] const CUcontext& get() const noexcept { return context_; }
    [[nodiscard]]       CUcontext& get()       noexcept { return context_; }

    [[nodiscard]] CUdevice device() const noexcept { return device_; }

private:
    CUcontext context_ = nullptr;
    CUdevice device_ = -1;
};

} // namespace thesis::cuda

#endif // __cplusplus
