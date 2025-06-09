#pragma once

#include "thesis/common/utils/types.h"
#include "thesis/host/cuda/buffer.h"
#include "thesis/host/utils/check.h"

#include <cuda.h>
#include <optix.h>

#include <cstddef>
#include <span>

namespace thesis::host::optix {

static constexpr uint BUILD_FLAGS = OPTIX_BUILD_FLAG_NONE;
// OPTIX_BUILD_FLAG_ALLOW_COMPACTION | OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;

class GAS {
   private:
    OptixDeviceContext context_ = nullptr;
    cuda::Buffer<std::byte> output_buffer_;
    OptixTraversableHandle gas_handle_ = 0;

   public:
    GAS() = default;

    GAS(GAS&&) noexcept = default;
    GAS& operator=(GAS&&) noexcept = default;

    GAS(const GAS&) = delete;
    GAS& operator=(const GAS&) = delete;

    GAS(OptixDeviceContext context, const OptixBuildInput& build_input, cudaStream_t stream,
        CUcontext ctx)
        : context_(context) {
        OptixAccelBuildOptions opts = {};
        opts.buildFlags = BUILD_FLAGS;
        opts.operation = OPTIX_BUILD_OPERATION_BUILD;

        OptixAccelBufferSizes sizes;
        OPTIX_CHECK(optixAccelComputeMemoryUsage(context_, &opts, &build_input, 1, &sizes));

        auto temp_buffer = cuda::Buffer<std::byte>::onDeviceOnly(sizes.tempSizeInBytes, ctx);
        output_buffer_ = cuda::Buffer<std::byte>::onDeviceOnly(sizes.outputSizeInBytes, ctx);

        OPTIX_CHECK(optixAccelBuild(context_, stream, &opts, &build_input, 1,
                                    reinterpret_cast<CUdeviceptr>(temp_buffer.device()),
                                    temp_buffer.size(),
                                    reinterpret_cast<CUdeviceptr>(output_buffer_.device()),
                                    output_buffer_.size(), &gas_handle_, nullptr, 0));
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return gas_handle_; }
};

class TriangleGAS {
   private:
    cuda::Buffer<float3> vertices_;
    cuda::Buffer<uint3> indices_;
    optix::GAS gas_;

   public:
    TriangleGAS() = default;

    TriangleGAS(TriangleGAS&&) noexcept = default;
    TriangleGAS& operator=(TriangleGAS&&) noexcept = default;

    TriangleGAS(const TriangleGAS&) = delete;
    TriangleGAS& operator=(const TriangleGAS&) = delete;

    TriangleGAS(cudaStream_t stream, OptixDeviceContext context, std::span<const float3> vertices,
                std::span<const uint3> indices, CUcontext ctx)
        : vertices_(vertices.data(), vertices.size(), ctx),
          indices_(indices.data(), indices.size(), ctx) {
        CUdeviceptr vertexBuffer = reinterpret_cast<CUdeviceptr>(vertices_.device());
        CUdeviceptr indexBuffer = reinterpret_cast<CUdeviceptr>(indices_.device());

        OptixBuildInput build_input = {};
        build_input.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;

        build_input.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
        build_input.triangleArray.vertexBuffers = &vertexBuffer;
        build_input.triangleArray.numVertices = static_cast<uint32_t>(vertices_.size());

        build_input.triangleArray.indexFormat = OPTIX_INDICES_FORMAT_UNSIGNED_INT3;
        build_input.triangleArray.indexBuffer = indexBuffer;
        build_input.triangleArray.numIndexTriplets = static_cast<uint32_t>(indices_.size());

        static constexpr uint32_t input_flags[1] = {OPTIX_GEOMETRY_FLAG_NONE};
        build_input.triangleArray.flags = input_flags;
        build_input.triangleArray.numSbtRecords = 1;

        gas_ = GAS(context, build_input, stream, ctx);
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return gas_.get(); }
};

}  // namespace thesis::host::optix