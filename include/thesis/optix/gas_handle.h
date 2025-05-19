#pragma once

#include "thesis/cuda/buffer.h"
#include "thesis/utils/check.h"

#include <cuda.h>
#include <optix.h>

#include <cstddef>
#include <span>

namespace thesis::optix {

class GASHandle {
   public:
    GASHandle() = default;

    GASHandle(OptixDeviceContext context, const OptixBuildInput& build_input, cudaStream_t stream)
        : context_(context) {
        OptixAccelBuildOptions accel_options = {};
        accel_options.buildFlags = OPTIX_BUILD_FLAG_NONE;
        accel_options.operation = OPTIX_BUILD_OPERATION_BUILD;

        OptixAccelBufferSizes gas_buffer_sizes;
        OPTIX_CHECK(optixAccelComputeMemoryUsage(context_, &accel_options, &build_input, 1,
                                                 &gas_buffer_sizes));

        auto temp_buffer = cuda::Buffer<std::byte>::onDeviceOnly(gas_buffer_sizes.tempSizeInBytes);
        output_buffer_ = cuda::Buffer<std::byte>::onDeviceOnly(gas_buffer_sizes.outputSizeInBytes);

        OPTIX_CHECK(optixAccelBuild(context_, stream, &accel_options, &build_input, 1,
                                    reinterpret_cast<CUdeviceptr>(temp_buffer.device()),
                                    temp_buffer.size(),
                                    reinterpret_cast<CUdeviceptr>(output_buffer_.device()),
                                    output_buffer_.size(), &gas_handle_, nullptr, 0));
    }

    GASHandle(GASHandle&&) noexcept = default;
    GASHandle& operator=(GASHandle&&) noexcept = default;

    GASHandle(const GASHandle&) = delete;
    GASHandle& operator=(const GASHandle&) = delete;

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return gas_handle_; }

   private:
    OptixDeviceContext context_ = nullptr;
    thesis::cuda::Buffer<std::byte> output_buffer_;
    OptixTraversableHandle gas_handle_ = 0;
};

class TriangleGAS {
   public:
    TriangleGAS() = default;

    TriangleGAS(TriangleGAS&&) noexcept = default;
    TriangleGAS& operator=(TriangleGAS&&) noexcept = default;

    TriangleGAS(const TriangleGAS&) = delete;
    TriangleGAS& operator=(const TriangleGAS&) = delete;

    TriangleGAS(cudaStream_t stream, OptixDeviceContext context, std::span<const float3> vertices,
                std::span<const uint3> indices = {})
        : vertices_(vertices.data(), vertices.size()), indices_(indices.data(), indices.size()) {
        CUdeviceptr vertexBuffer = reinterpret_cast<CUdeviceptr>(vertices_.device());
        CUdeviceptr indexBuffer = reinterpret_cast<CUdeviceptr>(indices_.device());

        OptixBuildInput build_input = {};
        build_input.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;

        build_input.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
        build_input.triangleArray.vertexBuffers = &vertexBuffer;
        build_input.triangleArray.numVertices = static_cast<uint32_t>(vertices_.size());

        if (!indices.empty()) {
            build_input.triangleArray.indexFormat = OPTIX_INDICES_FORMAT_UNSIGNED_INT3;
            build_input.triangleArray.indexBuffer = indexBuffer;
            build_input.triangleArray.numIndexTriplets = static_cast<uint32_t>(indices_.size());
        }

        static constexpr uint32_t input_flags[1] = {OPTIX_GEOMETRY_FLAG_NONE};
        build_input.triangleArray.flags = input_flags;
        build_input.triangleArray.numSbtRecords = 1;

        gas_ = GASHandle(context, build_input, stream);
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return gas_.get(); }

   private:
    cuda::Buffer<float3> vertices_;
    cuda::Buffer<uint3> indices_;
    optix::GASHandle gas_;
};

}  // namespace thesis::optix