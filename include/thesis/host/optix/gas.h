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

        OptixAccelBuildOptions opts = {};
        opts.buildFlags = BUILD_FLAGS;
        opts.operation = OPTIX_BUILD_OPERATION_BUILD;

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
        stream->synchronize();

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

template <class Shape>
class TriangleGAS {
    static_assert(std::is_same_v<decltype(Shape::NumVertices), const size_t>);
    static_assert(std::is_same_v<decltype(Shape::NumIndices), const size_t>);

    cuda::AsyncBuffer<float3> vertices_;
    cuda::AsyncBuffer<uint3> indices_;
    GAS gas_;

   public:
    TriangleGAS(const Shape& shape, CUcontext ctx, std::shared_ptr<cuda::Stream> stream)
        : vertices_(Shape::NumVertices, ctx, stream, cuda::AllocType::OnBoth),
          indices_(Shape::NumIndices, ctx, stream, cuda::AllocType::OnBoth),
          gas_(ctx, std::move(stream)) {
        const auto v_src = utils::data::reinterpretSpan<const float3>(shape.getVertices());
        std::memcpy(vertices_.host(), v_src.data(), v_src.size_bytes());
        vertices_.upload();

        const auto i_src = utils::data::reinterpretSpan<const uint3>(shape.getIndices());
        std::memcpy(indices_.host(), i_src.data(), i_src.size_bytes());
        indices_.upload();
    }

    TriangleGAS(TriangleGAS&&) noexcept = default;
    TriangleGAS& operator=(TriangleGAS&&) noexcept = default;
    TriangleGAS(const TriangleGAS&) = delete;
    TriangleGAS& operator=(const TriangleGAS&) = delete;

    /** Finalise: build the BLAS on the given OptiX context */
    void build(CUcontext cuda_ctx, OptixDeviceContext optix_ctx) {
        OptixBuildInput in = {};
        in.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;

        auto vbuf = vertices_.cu_device_ptr();
        in.triangleArray.vertexBuffers = &vbuf;
        in.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
        in.triangleArray.numVertices = static_cast<uint>(vertices_.size());

        auto ibuf = indices_.cu_device_ptr();
        in.triangleArray.indexBuffer = ibuf;
        in.triangleArray.indexFormat = OPTIX_INDICES_FORMAT_UNSIGNED_INT3;
        in.triangleArray.numIndexTriplets = static_cast<uint>(indices_.size());

        static constexpr uint flags[1] = {OPTIX_GEOMETRY_FLAG_NONE};
        in.triangleArray.flags = flags;
        in.triangleArray.numSbtRecords = 1;  // one header shared

        gas_.build(in, cuda_ctx, optix_ctx);
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return gas_.get(); }
};

using InstanceGAS = GAS;

}  // namespace thesis::host::optix
