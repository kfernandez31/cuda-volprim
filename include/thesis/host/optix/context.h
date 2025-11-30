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
#ifdef DEBUG
        opts.logCallbackLevel = static_cast<int>(LogLevel::Info);
        opts.validationMode = OPTIX_DEVICE_CONTEXT_VALIDATION_MODE_ALL;
#else
        opts.logCallbackLevel = static_cast<int>(LogLevel::Warning);
        opts.validationMode = OPTIX_DEVICE_CONTEXT_VALIDATION_MODE_OFF;
#endif

        OPTIX_CHECK(optixDeviceContextCreate(cu_ctx, &opts, &handle_));
        const char* log_level =
            opts.logCallbackLevel == static_cast<int>(LogLevel::Info) ? "Info" : "Warning";
        const char* validation =
            opts.validationMode == OPTIX_DEVICE_CONTEXT_VALIDATION_MODE_ALL ? "ALL" : "OFF";
        spdlog::debug("OptiX device context created (log level: {}, validation: {})", log_level,
                      validation);

        OPTIX_CHECK(optixDeviceContextSetCacheEnabled(handle_, 1));
        constexpr size_t MEGABYTES = 1024 * 1024;
        OPTIX_CHECK(
            optixDeviceContextSetCacheDatabaseSizes(handle_, 64 * MEGABYTES, 128 * MEGABYTES));
    }

    ~Context() { reset(); }

    Context(Context&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}

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
