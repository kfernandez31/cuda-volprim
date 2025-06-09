#pragma once

#include "thesis/common/utils/types.h"
#include "thesis/host/utils/data.h"
#include "thesis/host/cuda/buffer.h"
#include "thesis/host/utils/check.h"
#include "thesis/host/geometry/mesh.h"

#include <cuda.h>
#include <optix.h>

#include <cstddef>
#include <span>

namespace thesis::host::optix {

static constexpr uint BUILD_FLAGS = OPTIX_BUILD_FLAG_NONE;
// OPTIX_BUILD_FLAG_ALLOW_COMPACTION | OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;

class GAS {
   private:
    cuda::Buffer<std::byte> temp_, out_;
    cuda::Buffer<size_t> compacted_size_;
    OptixTraversableHandle gas_handle_ = 0;

   public:
    GAS(CUcontext ctx) : compacted_size_(cuda::Buffer<size_t>::onBoth(1, ctx)) {};

    GAS(GAS&&) noexcept = default;
    GAS& operator=(GAS&&) noexcept = default;

    GAS(const GAS&) = delete;
    GAS& operator=(const GAS&) = delete;

    void build(OptixBuildInput& input, cudaStream_t stream, CUcontext cuda_ctx, OptixDeviceContext optix_ctx) {
        OptixAccelBuildOptions opts = {};
        opts.buildFlags = BUILD_FLAGS;
        opts.operation = OPTIX_BUILD_OPERATION_BUILD;

        OptixAccelBufferSizes sizes;
        OPTIX_CHECK(optixAccelComputeMemoryUsage(optix_ctx, &opts, &input, 1, &sizes));

        temp_ = cuda::Buffer<std::byte>::onDeviceOnly(sizes.tempSizeInBytes, cuda_ctx);
        out_ = cuda::Buffer<std::byte>::onDeviceOnly(sizes.outputSizeInBytes, cuda_ctx);

        OPTIX_CHECK(optixAccelBuild(optix_ctx, stream, &opts, &input, 1,
                                    reinterpret_cast<CUdeviceptr>(temp_.device()), temp_.size(),
                                    reinterpret_cast<CUdeviceptr>(out_.device()), out_.size(),
                                    &gas_handle_, nullptr, 0));
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return gas_handle_; }
};

class TriangleGAS {
   private:
    GAS gas_;
    cuda::Buffer<float3> vertices_;
    cuda::Buffer<uint3> indices_;

   public:
    TriangleGAS() = default;

    TriangleGAS(size_t num_vertices, size_t num_indices, CUcontext ctx)
        // : vertices_(cuda::Buffer<float3>::onBoth(num_vertices, ctx))
        // , indices_(cuda::Buffer<uint3>::onBoth(num_indices, ctx))
        : gas_(ctx) {}

    TriangleGAS(TriangleGAS&&) noexcept = default;
    TriangleGAS& operator=(TriangleGAS&&) noexcept = default;

    TriangleGAS(const TriangleGAS&) = delete;
    TriangleGAS& operator=(const TriangleGAS&) = delete;

    // void appendGeometry(const geometry::Mesh& mesh) {
    //     auto vertex_offset = static_cast<uint>(vertices_.size());
    //     vertices_.push_back(utils::data::reinterpretSpan<float3, glm::vec3>(mesh.getVertices()));

    //     auto old_size = indices_.size();
    //     indices_.push_back(utils::data::reinterpretSpan<uint3, glm::uvec3>(mesh.getIndices()));

    //     std::transform(indices_.begin() + old_size, indices_.begin() + indices_.size(),
    //                    indices_.begin() + old_size, [=](uint3 tri) { return tri + vertex_offset; });
    // }

    void build(cudaStream_t stream, CUcontext cuda_ctx, OptixDeviceContext optix_ctx, 
        std::span<const float3> vs, 
        std::span<const uint3> is
    ) {
        vertices_ = cuda::Buffer<float3>::onDeviceOnly(vs, cuda_ctx);
        indices_ = cuda::Buffer<uint3>::onDeviceOnly(is, cuda_ctx);

        // spdlog::info("vertices_.uploadAll()");
        // vertices_.upload();

        // spdlog::info("indices_.uploadAll()");
        // indices_.upload();

        OptixBuildInput input = {};
        input.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;

        auto vertexBuffer = reinterpret_cast<CUdeviceptr>(vertices_.device());
        input.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
        input.triangleArray.vertexBuffers = &vertexBuffer;
        input.triangleArray.numVertices = static_cast<uint32_t>(vertices_.size());

        auto indexBuffer = reinterpret_cast<CUdeviceptr>(indices_.device());
        input.triangleArray.indexFormat = OPTIX_INDICES_FORMAT_UNSIGNED_INT3;
        input.triangleArray.indexBuffer = indexBuffer;
        input.triangleArray.numIndexTriplets = static_cast<uint32_t>(indices_.size());

        // clang-format off
        static constexpr uint32_t input_flags[1] = {OPTIX_GEOMETRY_FLAG_NONE};  // TODO(kacper): uint32_t?
        input.triangleArray.flags = input_flags;
        input.triangleArray.numSbtRecords = 1;

        spdlog::info("gas.build()");
        gas_.build(input, stream, cuda_ctx, optix_ctx);    
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return gas_.get(); }
};

}  // namespace thesis::host::optix
