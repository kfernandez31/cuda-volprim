#pragma once

#include "thesis/common/utils/types.h"
#include "thesis/host/cuda/buffer.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/utils/check.h"

#include <cuda.h>
#include <optix.h>

#include <memory>
#include <spdlog/spdlog.h>

namespace thesis::host::optix {

class AccelerationStructure {
   protected:
    cuda::Buffer<std::byte> temp_, out_;
    cuda::Buffer<size_t> compacted_size_;
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

        temp_ =
            cuda::Buffer<std::byte>(sz.tempSizeInBytes, cuda_ctx, cuda::AllocType::OnDeviceOnly);
        out_ =
            cuda::Buffer<std::byte>(sz.outputSizeInBytes, cuda_ctx, cuda::AllocType::OnDeviceOnly);

        OptixAccelEmitDesc* emit_desc_ptr = nullptr;
        OptixAccelEmitDesc emit{};
        if (build_flags & OPTIX_BUILD_FLAG_ALLOW_COMPACTION) {
            compacted_size_[0] = 0;
            compacted_size_.upload();
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
            compacted_size_.download();

            const auto compacted_size = compacted_size_[0];
            if (compacted_size > 0 && compacted_size < out_.size()) {
                spdlog::info("{} compaction issued ({} -> {} bytes)", structure_type, out_.size(),
                             compacted_size);
                out_ = cuda::Buffer<std::byte>(compacted_size, cuda_ctx,
                                               cuda::AllocType::OnDeviceOnly);
                OPTIX_CHECK(optixAccelCompact(optix_ctx, stream, handle_,
                                              reinterpret_cast<CUdeviceptr>(out_.device()),
                                              compacted_size, &handle_));
            }
        }
    }

   public:
    AccelerationStructure(CUcontext ctx, std::shared_ptr<cuda::Stream> stream)
        : compacted_size_(1, ctx, cuda::AllocType::OnBoth), stream_(std::move(stream)) {}

    AccelerationStructure(AccelerationStructure&&) noexcept = default;
    AccelerationStructure& operator=(AccelerationStructure&&) noexcept = default;

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return handle_; }
};

}  // namespace thesis::host::optix