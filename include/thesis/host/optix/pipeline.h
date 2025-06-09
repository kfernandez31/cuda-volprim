#pragma once

#include "thesis/common/utils/types.h"
#include "thesis/host/optix/logging.h"
#include "thesis/host/utils/check.h"

#include <optix_stubs.h>

#include <cstddef>
#include <utility>

namespace thesis::host::optix {

class Pipeline {
    OptixPipeline handle_ = nullptr;

   public:
    Pipeline() = default;

    Pipeline(OptixDeviceContext ctx, const OptixPipelineCompileOptions& pco,
             const OptixPipelineLinkOptions& plo, const OptixProgramGroup* groups,
             size_t num_groups) {
        OPTIX_CALL_LOGGED(optixPipelineCreate(ctx, &pco, &plo, groups,
                                              static_cast<uint>(num_groups), log.data(), &log_size,
                                              &handle_));
    }

    ~Pipeline() {
        if (handle_) {
            OPTIX_CHECK(optixPipelineDestroy(handle_));
        }
    }

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    Pipeline(Pipeline&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    Pipeline& operator=(Pipeline&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                OPTIX_CHECK(optixPipelineDestroy(handle_));
            }
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    void launch(CUstream stream, CUdeviceptr params, size_t params_size,
                const OptixShaderBindingTable& sbt, uint width, uint height, uint depth) const {
        OPTIX_CHECK(optixLaunch(handle_, stream, params, params_size, &sbt, width, height, depth));
    }

    [[nodiscard]] OptixPipeline get() const noexcept { return handle_; }
};

}  // namespace thesis::host::optix
