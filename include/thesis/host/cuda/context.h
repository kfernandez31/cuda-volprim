#pragma once

#include "thesis/host/utils/check.h"

#include <cuda.h>

#include <spdlog/spdlog.h>
#include <utility>

namespace thesis::host::cuda {

class Context {
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
    explicit Context(int device_ordinal = 0) {
        CU_CHECK(cuInit(0));
        CU_CHECK(cuDeviceGet(&device_, device_ordinal));
        CU_CHECK(cuCtxCreate(&context_, 0, device_));
        CU_CHECK(cuCtxSetCurrent(context_));

        char name[256];
        CU_CHECK(cuDeviceGetName(name, sizeof(name), device_));

        int major = 0, minor = 0;
        CU_CHECK(
            cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, device_));
        CU_CHECK(
            cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, device_));

        spdlog::info("Created CUDA context for device {} ({}), compute capability {}.{}",
                     device_ordinal, name, major, minor);
    }

    ~Context() { reset(); }

    Context(Context&& other) noexcept
        : context_(std::exchange(other.context_, nullptr)),
          device_(std::exchange(other.device_, -1)) {}

    Context& operator=(Context&& other) noexcept {
        if (this != &other) {
            reset();
            context_ = std::exchange(other.context_, nullptr);
            device_ = std::exchange(other.device_, -1);
        }
        return *this;
    }

    struct Guard {
        CUcontext prev = nullptr;

        Guard(CUcontext ctx) { CU_CHECK(cuCtxPushCurrent(ctx)); }
        ~Guard() { CU_CHECK_NOEXCEPT(cuCtxPopCurrent(&prev)); }

        Guard(Guard&& other) noexcept : prev(std::exchange(other.prev, nullptr)) {}
        Guard& operator=(Guard&& other) noexcept {
            if (this != &other) {
                prev = std::exchange(other.prev, nullptr);
            }
        }
    };

    [[nodiscard]] CUcontext get() const noexcept { return context_; }
    [[nodiscard]] CUdevice device() const noexcept { return device_; }
};

}  // namespace thesis::host::cuda
