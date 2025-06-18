#pragma once

#include "thesis/common/utils/types.h"
#include "thesis/host/cuda/async_buffer.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/geometry/mesh.h"
#include "thesis/host/utils/check.h"
#include "thesis/host/utils/data.h"

#include <cuda.h>
#include <optix.h>

#include <cstddef>
#include <memory>
#include <span>

namespace thesis::host::optix {

static constexpr uint BUILD_FLAGS =
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

    void build(const OptixBuildInput& input, CUcontext cuda_ctx, OptixDeviceContext optix_ctx,
               std::shared_ptr<cuda::Stream> stream) {
        OptixAccelBuildOptions opts = {};
        opts.buildFlags = BUILD_FLAGS;
        opts.operation = OPTIX_BUILD_OPERATION_BUILD;

        // TODO(kacper): I think this assumes that the input is already on device?
        stream->synchronize();  // TODO(kacper): needed? maybe use a 2nd stream?
        OptixAccelBufferSizes sizes;
        OPTIX_CHECK(optixAccelComputeMemoryUsage(optix_ctx, &opts, &input, 1, &sizes));

        temp_ = cuda::AsyncBuffer<std::byte>(sizes.tempSizeInBytes, cuda_ctx, stream,
                                             cuda::AllocType::OnDeviceOnly);
        out_ = cuda::AsyncBuffer<std::byte>(sizes.outputSizeInBytes, cuda_ctx, stream,
                                            cuda::AllocType::OnDeviceOnly);

        OptixAccelEmitDesc emit = {};
        emit.type = OPTIX_PROPERTY_TYPE_COMPACTED_SIZE;

        compacted_size_[0] = 0;  // initial value
        compacted_size_.upload();
        emit.result = compacted_size_.cu_device_ptr();

        OPTIX_CHECK(optixAccelBuild(optix_ctx, stream->get(), &opts, &input, 1,
                                    temp_.cu_device_ptr(), temp_.size(), out_.cu_device_ptr(),
                                    out_.size(), &handle_, &emit, 1));
        compacted_size_.download();

        // compact the GAS
        auto compacted_size = compacted_size_[0];
        if (compacted_size > 0) {
            spdlog::info("compacted_size = {}", compacted_size);
            out_ = cuda::AsyncBuffer<std::byte>(compacted_size, cuda_ctx, stream,
                                                cuda::AllocType::OnDeviceOnly);

            spdlog::info("compact()");
            OPTIX_CHECK(optixAccelCompact(optix_ctx, stream->get(), handle_,
                                          reinterpret_cast<CUdeviceptr>(out_.device()),
                                          compacted_size, &handle_));
        } else {
            spdlog::warn("GAS compaction skipped (compacted_size = 0)");
        }
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return handle_; }
};

class TriangleGAS {
   private:
    cuda::AsyncBuffer<float3> vertices_;
    cuda::AsyncBuffer<uint3> indices_;
    GAS gas_;

   public:
    TriangleGAS(size_t num_vertices, size_t num_indices, CUcontext ctx,
                std::shared_ptr<cuda::Stream> stream)
        : vertices_(num_vertices, ctx, stream, cuda::AllocType::OnBoth),
          indices_(num_indices, ctx, stream, cuda::AllocType::OnBoth),
          gas_(ctx, std::move(stream)) {
        spdlog::info("TriangleGAS ctor");
    }

    TriangleGAS(TriangleGAS&&) noexcept = default;
    TriangleGAS& operator=(TriangleGAS&&) noexcept = default;

    TriangleGAS(const TriangleGAS&) = delete;
    TriangleGAS& operator=(const TriangleGAS&) = delete;

    void build(CUcontext cuda_ctx, OptixDeviceContext optix_ctx,
               std::shared_ptr<cuda::Stream> stream, std::span<const float3> vs,
               std::span<const uint3> is) {
        // TODO(kacper): this could be potentially eliminated with an inplace creation of the vs/is
        // inside vertices/indices
        vertices_.set_and_upload(vs);
        indices_.set_and_upload(is);

        OptixBuildInput input = {};
        input.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;

        auto vertexBuffer = vertices_.cu_device_ptr();
        input.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
        input.triangleArray.vertexBuffers = &vertexBuffer;
        input.triangleArray.numVertices = static_cast<uint32_t>(vertices_.size());

        auto indexBuffer = indices_.cu_device_ptr();
        input.triangleArray.indexFormat = OPTIX_INDICES_FORMAT_UNSIGNED_INT3;
        input.triangleArray.indexBuffer = indexBuffer;
        input.triangleArray.numIndexTriplets = static_cast<uint32_t>(indices_.size());

        // clang-format off
        static constexpr uint32_t input_flags[1] = {OPTIX_GEOMETRY_FLAG_NONE};  // TODO(kacper): uint32_t?
        input.triangleArray.flags = input_flags;
        input.triangleArray.numSbtRecords = 1;

        spdlog::info("gas.build()");

        gas_.build(input, cuda_ctx, optix_ctx, std::move(stream));
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return gas_.get(); }
};

}  // namespace thesis::host::optix
