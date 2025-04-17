#pragma once

#ifdef __cplusplus

#include "thesis/utils/check.h"

#include <cuda.h>
#include <optix_stubs.h>

#include <string>
#include <utility>

// -------------------------
// Generic OptiX Handle
// -------------------------

namespace thesis::optix {

template <typename T, auto DestroyFn>
class Handle {
public:
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    Handle(Handle&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}

    Handle& operator=(Handle&& other) noexcept
    {
        if (this != &other) {
            if (handle_) {
                OPTIX_CHECK(DestroyFn(handle_));
            }

            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    ~Handle() noexcept
    {
        if (handle_) {
            OPTIX_CHECK(DestroyFn(handle_));
        }
    }

    [[nodiscard]] const T& get() const noexcept { return handle_; }
    [[nodiscard]]       T& get()       noexcept { return handle_; }

protected:
    Handle() noexcept = default;

private:
    T handle_ = 0;
};

// -------------------------
// Specific Handle Types
// -------------------------

class DeviceContextHandle : public Handle<OptixDeviceContext, optixDeviceContextDestroy> {
public:
    explicit DeviceContextHandle(
        const OptixDeviceContextOptions& dco,
        CUcontext cu_ctx = nullptr)
    {
        OPTIX_CHECK(optixDeviceContextCreate(cu_ctx, &dco, &get()));
    }
};

class ModuleHandle : public Handle<OptixModule, optixModuleDestroy> {
public:
    ModuleHandle(
        OptixDeviceContext ctx,
        const OptixModuleCompileOptions& mco,
        const OptixPipelineCompileOptions& pco,
        const std::string& ptx)
    {
        OPTIX_CALL_LOGGED(optixModuleCreate(ctx, &mco, &pco, ptx.c_str(), ptx.size(), log.data(), &log_size, &get()));
    }
};

class ProgramGroupHandle : public Handle<OptixProgramGroup, optixProgramGroupDestroy> {
public:
    ProgramGroupHandle(
        OptixDeviceContext ctx,
        const OptixProgramGroupDesc& desc)
    {
        const OptixProgramGroupOptions pg_options = {};
        OPTIX_CALL_LOGGED(optixProgramGroupCreate(ctx, &desc, 1, &pg_options, log.data(), &log_size, &get()));
    }
};

class PipelineHandle : public Handle<OptixPipeline, optixPipelineDestroy> {
public:
    PipelineHandle(
        OptixDeviceContext ctx,
        const OptixPipelineCompileOptions& pco,
        const OptixPipelineLinkOptions& plo,
        const OptixProgramGroup* groups,
        unsigned int num_groups)
    {
        OPTIX_CALL_LOGGED(optixPipelineCreate(ctx, &pco, &plo, groups, num_groups, log.data(), &log_size, &get()));
    }

    void launch(
        CUstream stream,
        CUdeviceptr params,
        size_t params_size,
        const OptixShaderBindingTable& sbt,
        unsigned int width,
        unsigned int height,
        unsigned int depth = 1
    )
    {
        OPTIX_CHECK(optixLaunch(get(), stream, params, params_size, &sbt, width, height, depth));
    }
};

} // namespace thesis::optix

#endif // __cplusplus
