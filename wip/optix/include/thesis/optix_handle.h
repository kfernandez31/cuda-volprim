#pragma once

#include "check.h"

#include <optix_stubs.h>

#include <iostream>
#include <string>
#include <utility>

// -------------------------
// Generic OptiX Handle
// -------------------------

template <typename T, auto DestroyFn>
class OptixHandle {
public:
    OptixHandle() noexcept = default;

    OptixHandle(const OptixHandle&) = delete;
    OptixHandle& operator=(const OptixHandle&) = delete;

    OptixHandle(OptixHandle&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}

    OptixHandle& operator=(OptixHandle&& other) noexcept
    {
        if (this != &other) {
            if (handle_) OPTIX_CHECK(DestroyFn(handle_));
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    ~OptixHandle() noexcept {
        if (handle_) OPTIX_CHECK(DestroyFn(handle_));
    }

    operator T() const noexcept { return handle_; }

protected:
    T handle_ = nullptr;
};

// -------------------------
// Specific Handle Types
// -------------------------

class OptixDeviceContextHandle : public OptixHandle<OptixDeviceContext, optixDeviceContextDestroy> {
public:
    OptixDeviceContextHandle(CUcontext cuCtx = 0)
    {
        OPTIX_CHECK(optixDeviceContextCreate(cuCtx, nullptr, &handle_));
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
        OPTIX_CALL_LOGGED(optixModuleCreateFromPTX(ctx, &mco, &pco, ptx.c_str(), ptx.size(), log, &logSize, &handle_));
    }
};

class OptixProgramGroupHandle : public OptixHandle<OptixProgramGroup, optixProgramGroupDestroy> {
public:
    OptixProgramGroupHandle(
        OptixDeviceContext ctx,
        const OptixProgramGroupDesc& desc)
    {
        OPTIX_CALL_LOGGED(optixProgramGroupCreate(ctx, &desc, 1, nullptr, log, &logSize, &handle_));
    }
};

class OptixPipelineHandle : public OptixHandle<OptixPipeline, optixPipelineDestroy> {
public:
    OptixPipelineHandle(
        OptixDeviceContext ctx,
        const OptixPipelineCompileOptions& pco,
        const OptixPipelineLinkOptions& plo,
        const OptixProgramGroup* groups,
        unsigned int numGroups)
    {
        OPTIX_CALL_LOGGED(optixPipelineCreate(ctx, &pco, &plo, groups, numGroups, log, &logSize, &handle_));
    }
};
