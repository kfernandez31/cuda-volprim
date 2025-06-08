#pragma once

#include "thesis/host/utils/check.h"

#include <cuda.h>

#include <utility>

namespace thesis::host::cuda {

class ContextHandle {
   private:
    CUcontext context_ = nullptr;
    CUdevice device_ = -1;

    void reset() noexcept {
        if (context_) {
            CU_CHECK_NOEXCEPT(cuCtxDestroy(context_));
            context_ = nullptr;
            device_ = -1;
        }
    }

   public:
    explicit ContextHandle(int device_ordinal = 0) {
        CU_CHECK(cuInit(0));
        CU_CHECK(cuDeviceGet(&device_, device_ordinal));
        CU_CHECK(cuCtxCreate(&context_, 0, device_));
        CU_CHECK(cuCtxSetCurrent(context_));
    }

    ~ContextHandle() { reset(); }

    ContextHandle(ContextHandle&& other) noexcept
        : context_(std::exchange(other.context_, nullptr)),
          device_(std::exchange(other.device_, -1)) {}

    ContextHandle& operator=(ContextHandle&& other) noexcept {
        if (this != &other) {
            reset();
            context_ = std::exchange(other.context_, nullptr);
            device_ = std::exchange(other.device_, -1);
        }
        return *this;
    }

    ContextHandle(const ContextHandle&) = delete;
    ContextHandle& operator=(const ContextHandle&) = delete;

    [[nodiscard]] CUcontext get() const noexcept { return context_; }
    [[nodiscard]] CUdevice device() const noexcept { return device_; }
};

}  // namespace thesis::host::cuda
