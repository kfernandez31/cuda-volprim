#pragma once

#include "thesis/host/optix/logging.h"
#include "thesis/host/utils/check.h"

#include <cuda.h>
#include <optix_stubs.h>

#include <spdlog/spdlog.h>
#include <utility>

namespace thesis::host::optix {

class Context {
    OptixDeviceContext handle_ = nullptr;

   public:
    Context(CUcontext cu_ctx) {
        OPTIX_CHECK(optixInit());

        OptixDeviceContextOptions opts = {};
        opts.logCallbackFunction = &contextLogCb;
        opts.logCallbackLevel = static_cast<int>(LogLevel::Warning);
// #ifdef DEBUG
        opts.validationMode = OPTIX_DEVICE_CONTEXT_VALIDATION_MODE_ALL;
// #else
        // opts.validationMode = OPTIX_DEVICE_CONTEXT_VALIDATION_MODE_OFF;
// #endif

        OPTIX_CHECK(optixDeviceContextCreate(cu_ctx, &opts, &handle_));
        spdlog::info(
            "OptiX device context created (validation mode: {})",
            opts.validationMode == OPTIX_DEVICE_CONTEXT_VALIDATION_MODE_ALL ? "ALL" : "OFF");

        OPTIX_CHECK(optixDeviceContextSetCacheEnabled(handle_, 1));
        OPTIX_CHECK(optixDeviceContextSetCacheDatabaseSizes(handle_, 64 * 1024 * 1024,
                                                            128 * 1024 * 1024));  // 64MB, 128MB
    }

    ~Context() { reset(); }

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    Context(Context&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    Context& operator=(Context&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] OptixDeviceContext get() const noexcept { return handle_; }

   private:
    void reset() {
        if (handle_) {
            OPTIX_CHECK(optixDeviceContextDestroy(handle_));
        }
    }
};

}  // namespace thesis::host::optix