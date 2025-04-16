#pragma once

#ifdef __cplusplus

#include "check.h"

#include <cuda.h>
#include <optix_stubs.h>

#include <string>
#include <utility>

// -------------------------
// Generic OptiX Handle
// -------------------------

namespace thesis {

template <typename T, auto DestroyFn>
class OptixHandle {
public:
    OptixHandle(const OptixHandle&) = delete;
    OptixHandle& operator=(const OptixHandle&) = delete;

    OptixHandle(OptixHandle&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}

    OptixHandle& operator=(OptixHandle&& other) noexcept
    {
        if (this != &other) {
            if (handle_) {
                OPTIX_CHECK(DestroyFn(handle_));
            }
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    ~OptixHandle() noexcept
    {
        if (handle_) {
            OPTIX_CHECK(DestroyFn(handle_));
        }
    }

    [[nodiscard]] const T& get() const noexcept { return handle_; }
    [[nodiscard]]       T& get()       noexcept { return handle_; }

protected:
    OptixHandle() noexcept = default;

private:
    T handle_ = 0;
};

// -------------------------
// Specific Handle Types
// -------------------------

class OptixDeviceContextHandle : public OptixHandle<OptixDeviceContext, optixDeviceContextDestroy> {
public:
    explicit OptixDeviceContextHandle(
        const OptixDeviceContextOptions& dco,
        CUcontext cu_ctx = nullptr)
    {
        OPTIX_CHECK(optixDeviceContextCreate(cu_ctx, &dco, &get()));
    }
};

class OptixModuleHandle : public OptixHandle<OptixModule, optixModuleDestroy> {
public:
    OptixModuleHandle(
        OptixDeviceContext ctx,
        const OptixModuleCompileOptions& mco,
        const OptixPipelineCompileOptions& pco,
        const std::string& ptx)
    {
        OPTIX_CALL_LOGGED(optixModuleCreate(ctx, &mco, &pco, ptx.c_str(), ptx.size(), log.data(), &log_size, &get()));
    }
};

class OptixProgramGroupHandle : public OptixHandle<OptixProgramGroup, optixProgramGroupDestroy> {
public:
    OptixProgramGroupHandle(
        OptixDeviceContext ctx,
        const OptixProgramGroupDesc& desc)
    {
        const OptixProgramGroupOptions pg_options = {};
        OPTIX_CALL_LOGGED(optixProgramGroupCreate(ctx, &desc, 1, &pg_options, log.data(), &log_size, &get()));
    }
};

class OptixPipelineHandle : public OptixHandle<OptixPipeline, optixPipelineDestroy> {
public:
    OptixPipelineHandle(
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
        const OptixShaderBindingTable* sbt,
        unsigned int width,
        unsigned int height,
        unsigned int depth = 1
    )
    {
        OPTIX_CHECK(optixLaunch(get(), stream, params, params_size, sbt, width, height, depth));
    }
};

} // namespace thesis

#endif // __cplusplus
