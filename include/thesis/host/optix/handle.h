#pragma once

#include "thesis/host/utils/check.h"
#include "thesis/common/utils/types.h"

#include <cuda.h>
#include <optix_stubs.h>

#include <string_view>
#include <utility>

// -------------------------
// Generic OptiX Handle
// -------------------------

namespace thesis::optix {

template <typename T, auto DestroyFn>
class Handle {
   protected:
    T handle_ = 0;

   public:
    Handle() = default;

    Handle(Handle&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                OPTIX_CHECK(DestroyFn(handle_));
            }

            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    ~Handle() noexcept {
        if (handle_) {
            OPTIX_CHECK(DestroyFn(handle_));
        }
    }

    [[nodiscard]] const T& get() const noexcept { return handle_; }
};

// -------------------------
// Specific Handle Types
// -------------------------

class DeviceContextHandle : public Handle<OptixDeviceContext, optixDeviceContextDestroy> {
   public:
    DeviceContextHandle(const OptixDeviceContextOptions& dco, CUcontext cu_ctx) {
        OPTIX_CHECK(optixDeviceContextCreate(cu_ctx, &dco, &handle_));
    }
};

class ModuleHandle : public Handle<OptixModule, optixModuleDestroy> {
   public:
    ModuleHandle() = default;
    ModuleHandle(OptixDeviceContext ctx, const OptixModuleCompileOptions& mco,
                 const OptixPipelineCompileOptions& pco, std::string_view ptx) {
        OPTIX_CALL_LOGGED(optixModuleCreate(ctx, &mco, &pco, ptx.data(), ptx.size(), log.data(),
                                            &log_size, &handle_));
    }
};

class ProgramGroupHandle : public Handle<OptixProgramGroup, optixProgramGroupDestroy> {
   public:
    ProgramGroupHandle() = default;
    ProgramGroupHandle(OptixDeviceContext ctx, const OptixProgramGroupDesc& desc) {
        const OptixProgramGroupOptions pg_options = {};
        OPTIX_CALL_LOGGED(
            optixProgramGroupCreate(ctx, &desc, 1, &pg_options, log.data(), &log_size, &handle_));
    }
};

class PipelineHandle : public Handle<OptixPipeline, optixPipelineDestroy> {
   public:
    PipelineHandle() = default;
    PipelineHandle(OptixDeviceContext ctx, const OptixPipelineCompileOptions& pco,
                   const OptixPipelineLinkOptions& plo, const OptixProgramGroup* groups,
                   size_t num_groups) {
        OPTIX_CALL_LOGGED(optixPipelineCreate(ctx, &pco, &plo, groups,
                                              static_cast<uint>(num_groups), log.data(),
                                              &log_size, &handle_));
    }

    void launch(CUstream stream, CUdeviceptr params, size_t params_size,
                const OptixShaderBindingTable& sbt, uint width, uint height,
                uint depth) const {
        OPTIX_CHECK(optixLaunch(handle_, stream, params, params_size, &sbt, width, height, depth));
    }
};

}  // namespace thesis::optix
