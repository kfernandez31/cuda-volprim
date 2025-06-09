#pragma once

#include "thesis/host/utils/check.h"
#include "thesis/host/optix/logging.h"

#include <optix_stubs.h>
#include <string_view>
#include <utility>

namespace thesis::host::optix {

class Module {
    OptixModule handle_ = nullptr;

   public:
    Module() = default;

    Module(OptixDeviceContext ctx, const OptixModuleCompileOptions& mco,
                 const OptixPipelineCompileOptions& pco, std::string_view ptx) {
        OPTIX_CALL_LOGGED(optixModuleCreate(ctx, &mco, &pco, ptx.data(), ptx.size(), log.data(),
                                            &log_size, &handle_));
    }

    ~Module() {
        if (handle_) {
            OPTIX_CHECK(optixModuleDestroy(handle_));
        }
    }

    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;

    Module(Module&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}

    Module& operator=(Module&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                OPTIX_CHECK(optixModuleDestroy(handle_));
            }
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] OptixModule get() const noexcept { return handle_; }
};

}  // namespace thesis::host::optix