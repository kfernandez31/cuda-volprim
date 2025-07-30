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

    void build(const OptixBuildInput& input, CUcontext cuda_ctx, OptixDeviceContext optix_ctx) {
        const auto& stream = compacted_size_.get_context_param();

        OptixAccelBuildOptions opts{};
        opts.buildFlags = BUILD_FLAGS;
        opts.operation = OPTIX_BUILD_OPERATION_BUILD;

        OptixAccelBufferSizes sz{};
        OPTIX_CHECK(optixAccelComputeMemoryUsage(optix_ctx, &opts, &input, 1, &sz));

        temp_ = cuda::AsyncBuffer<std::byte>(sz.tempSizeInBytes, cuda_ctx, stream,
                                             cuda::AllocType::OnDeviceOnly);
        out_ = cuda::AsyncBuffer<std::byte>(sz.outputSizeInBytes, cuda_ctx, stream,
                                            cuda::AllocType::OnDeviceOnly);

        OptixAccelEmitDesc emit{};
        emit.type = OPTIX_PROPERTY_TYPE_COMPACTED_SIZE;

        compacted_size_[0] = 0;  // initial value
        compacted_size_.upload();
        emit.result = compacted_size_.cu_device_ptr();

        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
        OPTIX_CHECK(optixAccelBuild(optix_ctx, stream->get(), &opts, &input, 1,
                                    temp_.cu_device_ptr(), temp_.size(), out_.cu_device_ptr(),
                                    out_.size(), &handle_, &emit, 1));
        compacted_size_.download();
        // compact the GAS
        const auto compacted_size = compacted_size_[0];
        if (compacted_size > 0 && compacted_size < out_.size()) {
            spdlog::info("GAS compaction issued ({} -> {} bytes)", out_.size(), compacted_size);
            out_ = cuda::AsyncBuffer<std::byte>(compacted_size, cuda_ctx, stream,
                                                cuda::AllocType::OnDeviceOnly);
            OPTIX_CHECK(optixAccelCompact(optix_ctx, stream->get(), handle_,
                                          reinterpret_cast<CUdeviceptr>(out_.device()),
                                          compacted_size, &handle_));
        } else {
            spdlog::warn("GAS compaction skipped (compacted_size = 0)");
        }
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return handle_; }
};

class SphereGAS {
    cuda::AsyncBuffer<float3> centers_; // TODO(kacper): compact into one buffer
    cuda::AsyncBuffer<float> radii_;
    GAS gas_;

   public:
    SphereGAS(CUcontext ctx, std::shared_ptr<cuda::Stream> stream)
        : centers_(1, ctx, stream, cuda::AllocType::OnBoth)
        , radii_(1, ctx, stream, cuda::AllocType::OnBoth)
        , gas_(ctx, std::move(stream)) {}

    void build(CUcontext cuda_ctx, OptixDeviceContext optix_ctx) {
        const auto center = make_float3(0.0f, 0.0f, 0.0f);
        centers_.host()[0] = center;
        centers_.upload();

        const auto radius = 1.0f;
        radii_.host()[0] = radius;
        radii_.upload();
        
        spdlog::info("Building sphere GAS: center=({},{},{}), radius={}", 
                     center.x, center.y, center.z, radius);

        radii_.get_context_param()->synchronize();
        spdlog::info("sync'd stream to satisfy Claude");

        static constexpr uint geomFlags[1] = {OPTIX_GEOMETRY_FLAG_NONE};

        OptixBuildInput in{};
        in.type = OPTIX_BUILD_INPUT_TYPE_SPHERES;

        auto vbuf = centers_.cu_device_ptr();
        in.sphereArray.vertexBuffers = &vbuf;
        in.sphereArray.numVertices = 1;
        in.sphereArray.vertexStrideInBytes = sizeof(float3);
        
        auto rbuf = radii_.cu_device_ptr();
        in.sphereArray.radiusBuffers = &rbuf;
        in.sphereArray.radiusStrideInBytes = sizeof(float);
        in.sphereArray.singleRadius = 0;
        
        spdlog::info("Sphere buffers: vertexBuffer=0x{:x}, radiusBuffer=0x{:x}", 
                     vbuf, rbuf);
    
        in.sphereArray.flags = geomFlags;
        in.sphereArray.numSbtRecords = 1;
        in.sphereArray.primitiveIndexOffset = 0;  // Explicitly set primitive index offset

        gas_.build(in, cuda_ctx, optix_ctx);
        spdlog::info("Sphere GAS built, handle = 0x{:x}", gas_.get());
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return gas_.get(); }
};

}  // namespace thesis::host::optix
