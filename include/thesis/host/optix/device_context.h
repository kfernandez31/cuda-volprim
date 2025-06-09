#pragma once

#include "thesis/host/optix/logging.h"
#include "thesis/host/utils/check.h"

#include <cuda.h>
#include <optix_stubs.h>

#include <spdlog/spdlog.h>
#include <utility>

namespace thesis::host::optix {

class DeviceContext {
    OptixDeviceContext handle_ = nullptr;

   public:
    DeviceContext(CUcontext cu_ctx) {
        OPTIX_CHECK(optixInit());

        OptixDeviceContextOptions opts = {};
        opts.logCallbackFunction = &contextLogCb;
        opts.logCallbackLevel = static_cast<int>(LogLevel::Warning);
#ifdef DEBUG
        opts.validationMode = OPTIX_DEVICE_CONTEXT_VALIDATION_MODE_ALL;
#else
        opts.validationMode = OPTIX_DEVICE_CONTEXT_VALIDATION_MODE_OFF;
#endif

        OPTIX_CHECK(optixDeviceContextCreate(cu_ctx, &opts, &handle_));
        spdlog::info("Created OptixDeviceContext = {}", reinterpret_cast<uintptr_t>(handle_));
    }

    ~DeviceContext() {
        if (handle_) {
            OPTIX_CHECK(optixDeviceContextDestroy(handle_));
        }
    }

    DeviceContext(const DeviceContext&) = delete;
    DeviceContext& operator=(const DeviceContext&) = delete;

    DeviceContext(DeviceContext&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}

    DeviceContext& operator=(DeviceContext&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                OPTIX_CHECK(optixDeviceContextDestroy(handle_));
            }
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] OptixDeviceContext get() const noexcept { return handle_; }
};

}  // namespace thesis::host::optix