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
        const OptixProgramGroupOptions pg_options{};
        OPTIX_CALL_LOGGED(
            optixProgramGroupCreate(ctx, &desc, 1, &pg_options, log.data(), &log_size, &handle_));
    }

    ~ProgramGroup() { reset(); }

    ProgramGroup(ProgramGroup&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    ProgramGroup& operator=(ProgramGroup&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    static ProgramGroup createRaygen(OptixDeviceContext ctx, OptixModule module,
                                     const char* entry) {
        OptixProgramGroupDesc desc{};
        desc.kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
        desc.raygen.module = module;
        desc.raygen.entryFunctionName = entry;
        return {ctx, desc};
    }

    static ProgramGroup createMiss(OptixDeviceContext ctx, OptixModule module, const char* entry) {
        OptixProgramGroupDesc desc{};
        desc.kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
        desc.miss.module = module;
        desc.miss.entryFunctionName = entry;
        return {ctx, desc};
    }

    static ProgramGroup createHitgroup(OptixDeviceContext ctx, OptixModule anyhit_module,
                                       const char* entry_ah, OptixModule builtin_is_module) {
        OptixProgramGroupDesc desc{};
        desc.kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;

        desc.hitgroup.moduleAH = anyhit_module;
        desc.hitgroup.entryFunctionNameAH = entry_ah;

        desc.hitgroup.moduleIS = builtin_is_module;
        desc.hitgroup.entryFunctionNameIS = nullptr;  // Built-in, no name needed

        return {ctx, desc};
    }

    [[nodiscard]] OptixProgramGroup get() const noexcept { return handle_; }

   private:
    void reset() {
        if (handle_) {
            OPTIX_CHECK(optixProgramGroupDestroy(handle_));
        }
    }
};

}  // namespace thesis::host::optix
