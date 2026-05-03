#pragma once

#include "thesis/common/utils/types.h"
#include "thesis/host/cuda/async_buffer.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/utils/check.h"

#include <cuda.h>
#include <optix.h>

#include <memory>
#include <spdlog/spdlog.h>

namespace thesis::host::optix {

class AccelerationStructure {
   protected:
    cuda::AsyncBuffer<std::byte> temp_, out_;
    cuda::AsyncBuffer<size_t> compacted_size_;
    OptixTraversableHandle handle_ = 0;
    std::shared_ptr<cuda::Stream> stream_;

    void build_internal(const OptixBuildInput& input, CUcontext cuda_ctx,
                        OptixDeviceContext optix_ctx, uint build_flags,
                        const char* structure_type) {
        const auto stream = stream_->get();

        OptixAccelBuildOptions opts{};
        opts.buildFlags = build_flags;
        opts.operation = OPTIX_BUILD_OPERATION_BUILD;

        OptixAccelBufferSizes sz{};
        OPTIX_CHECK(optixAccelComputeMemoryUsage(optix_ctx, &opts, &input, 1, &sz));

        temp_ = cuda::AsyncBuffer<std::byte>(sz.tempSizeInBytes, cuda_ctx, stream_,
                                             cuda::AllocType::OnDeviceOnly);
        out_ = cuda::AsyncBuffer<std::byte>(sz.outputSizeInBytes, cuda_ctx, stream_,
                                            cuda::AllocType::OnDeviceOnly);

        OptixAccelEmitDesc* emit_desc_ptr = nullptr;
        OptixAccelEmitDesc emit{};
        if (build_flags & OPTIX_BUILD_FLAG_ALLOW_COMPACTION) {
            // Zero the device slot before optixAccelBuild emits into it.
            CUDA_CHECK(cudaMemsetAsync(compacted_size_.device(), 0, sizeof(size_t), stream));
            emit.type = OPTIX_PROPERTY_TYPE_COMPACTED_SIZE;
            emit.result = compacted_size_.cu_device_ptr();
            emit_desc_ptr = &emit;
        }

        OPTIX_CHECK(optixAccelBuild(optix_ctx, stream, &opts, &input, 1, temp_.cu_device_ptr(),
                                    temp_.size(), out_.cu_device_ptr(), out_.size(), &handle_,
                                    emit_desc_ptr, emit_desc_ptr ? 1 : 0));

        if (!(build_flags & OPTIX_BUILD_FLAG_ALLOW_COMPACTION)) {
            spdlog::debug("{} compaction not requested", structure_type);
        } else {
            stream_->synchronize();
            // 8-byte readback after the sync above — synchronous cudaMemcpy is
            // simpler than another async + sync round-trip and avoids the 4 KB
            // pinned-page tax of an OnBoth host allocation for a single size_t.
            size_t compacted_size = 0;
            CUDA_CHECK(cudaMemcpy(&compacted_size, compacted_size_.device(), sizeof(size_t),
                                  cudaMemcpyDeviceToHost));
            if (compacted_size > 0 && compacted_size < out_.size()) {
                spdlog::info("{} compaction issued ({} -> {} bytes)", structure_type, out_.size(),
                             compacted_size);
                // Move the original (uncompacted) buffer into a local so it
                // outlives optixAccelCompact. Without this, `out_ = ...` below
                // would queue cudaFreeAsync on the original memory FIRST, then
                // optixAccelCompact (queued AFTER on the same stream) would
                // read from already-freed memory via `handle_`.
                auto old_out = std::move(out_);
                out_ = cuda::AsyncBuffer<std::byte>(compacted_size, cuda_ctx, stream_,
                                                    cuda::AllocType::OnDeviceOnly);
                OPTIX_CHECK(optixAccelCompact(optix_ctx, stream, handle_,
                                              reinterpret_cast<CUdeviceptr>(out_.device()),
                                              compacted_size, &handle_));
                // `old_out` destructs here — its cudaFreeAsync is queued on
                // `stream_` AFTER optixAccelCompact, so the read happens before
                // the free.
            }
        }

        // temp_ is build scratch — neither optixAccelBuild nor optixAccelCompact references it
        // afterwards. Stream-ordered async free here is correctly sequenced after both.
        temp_ = cuda::AsyncBuffer<std::byte>();
    }

   public:
    AccelerationStructure(CUcontext ctx, std::shared_ptr<cuda::Stream> stream)
        : compacted_size_(1, ctx, stream, cuda::AllocType::OnDeviceOnly),
          stream_(std::move(stream)) {}

    AccelerationStructure(AccelerationStructure&&) noexcept = default;
    AccelerationStructure& operator=(AccelerationStructure&&) noexcept = default;

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return handle_; }
};

}  // namespace thesis::host::optix