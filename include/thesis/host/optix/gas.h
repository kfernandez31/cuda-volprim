#pragma once

#include "thesis/common/utils/types.h"
#include "thesis/host/cuda/async_buffer.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/geometry/mesh.h"
#include "thesis/host/utils/check.h"
#include "thesis/host/utils/data.h"

#include <cuda.h>
#include <optix.h>
#include <vector_types.h>

#include <cstddef>
#include <memory>
#include <span>
#include <spdlog/spdlog.h>

namespace thesis::host::optix {

inline constexpr uint BUILD_FLAGS =
    OPTIX_BUILD_FLAG_ALLOW_COMPACTION | OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;

class GAS {
   private:
    cuda::AsyncBuffer<std::byte> temp_, out_;
    cuda::AsyncBuffer<size_t> compacted_size_;
    OptixTraversableHandle handle_ = 0;

   public:
    GAS(CUcontext ctx, std::shared_ptr<cuda::Stream> stream)
        : compacted_size_(1, ctx, std::move(stream), cuda::AllocType::OnBoth) {}

    GAS(GAS&&) noexcept = default;
    GAS& operator=(GAS&&) noexcept = default;

    GAS(const GAS&) = delete;
    GAS& operator=(const GAS&) = delete;

    void build(const OptixBuildInput& input, CUcontext cuda_ctx, OptixDeviceContext optix_ctx) {
        const auto& stream = compacted_size_.get_context_param();

        OptixAccelBuildOptions opts{};
        opts.buildFlags = BUILD_FLAGS;
        opts.operation = OPTIX_BUILD_OPERATION_BUILD;

        OptixAccelBufferSizes sz{};
        OPTIX_CHECK(optixAccelComputeMemoryUsage(optix_ctx, &opts, &input, 1, &sz));

        spdlog::info("hello A");

        temp_ = cuda::AsyncBuffer<std::byte>(sz.tempSizeInBytes, cuda_ctx, stream,
                                             cuda::AllocType::OnDeviceOnly);
        out_ = cuda::AsyncBuffer<std::byte>(sz.outputSizeInBytes, cuda_ctx, stream,
                                            cuda::AllocType::OnDeviceOnly);

        OptixAccelEmitDesc emit{};
        emit.type = OPTIX_PROPERTY_TYPE_COMPACTED_SIZE;

        compacted_size_[0] = 0;  // initial value
        compacted_size_.upload();
        emit.result = compacted_size_.cu_device_ptr();

        spdlog::info("hello B");

        OPTIX_CHECK(optixAccelBuild(optix_ctx, stream->get(), &opts, &input, 1,
                                    temp_.cu_device_ptr(), temp_.size(), out_.cu_device_ptr(),
                                    out_.size(), &handle_, &emit, 1));
        spdlog::info("hello C");
        compacted_size_.download();
        spdlog::info("hello D");
        stream->synchronize();
        spdlog::info("hello E");
        // compact the GAS
        const auto compacted_size = compacted_size_[0];
        spdlog::info("hello F");
        if (compacted_size > 0 && compacted_size < out_.size()) {
            spdlog::info("GAS compaction issued ({} → {} bytes)", out_.size(), compacted_size);
            out_ = cuda::AsyncBuffer<std::byte>(compacted_size, cuda_ctx, stream,
                                                cuda::AllocType::OnDeviceOnly);
            spdlog::info("hello G");
            OPTIX_CHECK(optixAccelCompact(optix_ctx, stream->get(), handle_,
                                          reinterpret_cast<CUdeviceptr>(out_.device()),
                                          compacted_size, &handle_));
            spdlog::info("hello H");
        } else {
            spdlog::warn("GAS compaction skipped (compacted_size = 0)");
        }
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return handle_; }
};

class SphereGAS {
    cuda::AsyncBuffer<float4> spheres_;  // (cx,cy,cz,r)
    GAS gas_;

   public:
    SphereGAS(size_t count, CUcontext ctx, std::shared_ptr<cuda::Stream> stream)
        : spheres_(count, ctx, stream, cuda::AllocType::OnBoth), gas_(ctx, std::move(stream)) {}

    [[nodiscard]] float4* host() noexcept { return spheres_.host(); }

    void build(CUcontext cuda_ctx, OptixDeviceContext optix_ctx) {
        spheres_.upload();

        static constexpr unsigned int geomFlags[1] = {OPTIX_GEOMETRY_FLAG_NONE};

        OptixBuildInput in{};
        in.type = OPTIX_BUILD_INPUT_TYPE_SPHERES;

        /* centres ------------------------------------------------------- */
        auto dptr = spheres_.cu_device_ptr();
        in.sphereArray.vertexBuffers = &dptr;
        in.sphereArray.numVertices = static_cast<unsigned>(spheres_.size());
        in.sphereArray.vertexStrideInBytes = sizeof(float4);

        spdlog::info("spheres size: {}", spheres_.size());
        spdlog::info("device ptr: {}", static_cast<void*>(spheres_.device()));

        /* radii – stored in w component, so no separate buffer ---------- */
        in.sphereArray.radiusBuffers = nullptr;
        in.sphereArray.singleRadius = 0;         // one radius per sphere
        in.sphereArray.radiusStrideInBytes = 0;  // ignored

        /* misc ---------------------------------------------------------- */
        in.sphereArray.flags = geomFlags;
        in.sphereArray.numSbtRecords = 1;

        /* disable per-primitive SBT index offsets ----------------------- */
        in.sphereArray.sbtIndexOffsetBuffer = 0;
        in.sphereArray.sbtIndexOffsetSizeInBytes = 0;
        in.sphereArray.sbtIndexOffsetStrideInBytes = 0;
        in.sphereArray.primitiveIndexOffset = 0;

        gas_.build(in, cuda_ctx, optix_ctx);
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return gas_.get(); }
};

using InstanceGAS = GAS;

}  // namespace thesis::host::optix
