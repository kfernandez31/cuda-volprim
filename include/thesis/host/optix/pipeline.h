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

        // TODO: The default continuation stack size may be insufficient for large scenes
        // (2048+ Gaussians). sample_scattering_event allocates ~32KB of stack per thread
        // (hit_buffer 8KB + events 16KB + cached_exits 8KB). This causes "illegal memory
        // access" on stress tests. Compute the required size and set explicitly:
        //
        // OptixStackSizes stack_sizes{};
        // for (size_t i = 0; i < num_groups; ++i) {
        //     OPTIX_CHECK(optixProgramGroupGetStackSize(groups[i], &stack_sizes));
        // }
        // OPTIX_CHECK(optixPipelineSetStackSize(
        //     handle_,
        //     /* directCallableStackSizeFromTraversal = */ 0,
        //     /* directCallableStackSizeFromState = */ 0,
        //     /* continuationStackSize = */ 65536,  // 64KB, tune empirically
        //     /* maxTraversableGraphDepth = */ 2     // IAS -> GAS
        // ));
    }

    ~Pipeline() {
        if (handle_) {
            OPTIX_CHECK(optixPipelineDestroy(handle_));
        }
    }

    Pipeline(Pipeline&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}

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
                const OptixShaderBindingTable& sbt, size_t width, size_t height,
                size_t depth) const {
        OPTIX_CHECK(optixLaunch(handle_, stream, params, params_size, &sbt,
                                static_cast<uint>(width), static_cast<uint>(height),
                                static_cast<uint>(depth)));
    }

    [[nodiscard]] OptixPipeline get() const noexcept { return handle_; }
};

}  // namespace thesis::host::optix
