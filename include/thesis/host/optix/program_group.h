#pragma once

#include "thesis/host/optix/logging.h"
#include "thesis/host/utils/check.h"

#include <optix_stubs.h>

#include <utility>

namespace thesis::host::optix {

class ProgramGroup {
    OptixProgramGroup handle_ = nullptr;

   public:
    ProgramGroup() = default;

    ProgramGroup(OptixDeviceContext ctx, const OptixProgramGroupDesc& desc) {
        const OptixProgramGroupOptions pg_options = {};
        OPTIX_CALL_LOGGED(
            optixProgramGroupCreate(ctx, &desc, 1, &pg_options, log.data(), &log_size, &handle_));
    }

    ~ProgramGroup() {
        if (handle_) {
            OPTIX_CHECK(optixProgramGroupDestroy(handle_));
        }
    }

    ProgramGroup(const ProgramGroup&) = delete;
    ProgramGroup& operator=(const ProgramGroup&) = delete;

    ProgramGroup(ProgramGroup&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    ProgramGroup& operator=(ProgramGroup&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                OPTIX_CHECK(optixProgramGroupDestroy(handle_));
            }
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] OptixProgramGroup get() const noexcept { return handle_; }
};

}  // namespace thesis::host::optix