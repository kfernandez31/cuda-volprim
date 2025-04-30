#pragma once

#include <span>

#include <optix.h>
#include <cuda.h>

#include "thesis/utils/check.h"

#include <cstddef>

namespace thesis::optix {

class GASHandle {
    public:
        GASHandle(OptixDeviceContext context, const OptixBuildInput& build_input)
            : context_(context)
        {
            OptixAccelBuildOptions accel_options = {};
            accel_options.buildFlags = OPTIX_BUILD_FLAG_NONE;
            accel_options.operation = OPTIX_BUILD_OPERATION_BUILD;
    
            OptixAccelBufferSizes gas_buffer_sizes;
            OPTIX_CHECK(optixAccelComputeMemoryUsage(context_, &accel_options, &build_input, 1, &gas_buffer_sizes));
    
            auto temp_buffer = thesis::cuda::UploadBuffer<std::byte>::createEmpty(gas_buffer_sizes.tempSizeInBytes);
            output_buffer_ = thesis::cuda::UploadBuffer<std::byte>::createEmpty(gas_buffer_sizes.outputSizeInBytes);
    
            OPTIX_CHECK(optixAccelBuild(
                context_,
                /*stream*/ 0,
                &accel_options,
                &build_input,
                1,
                temp_buffer.get(), 
                temp_buffer.size(),
                output_buffer_.get(), 
                output_buffer_.size(),
                &gas_handle_,
                nullptr,
                0
            ));
        }
    
        // Enable move
        GASHandle(GASHandle&&) noexcept = default;
        GASHandle& operator=(GASHandle&&) noexcept = default;
    
        // Disable copy
        GASHandle(const GASHandle&) = delete;
        GASHandle& operator=(const GASHandle&) = delete;
    
        ~GASHandle() = default;
    
        [[nodiscard]] const OptixTraversableHandle& get() const noexcept { return gas_handle_; }
        [[nodiscard]] OptixTraversableHandle& get() noexcept { return gas_handle_; }
    
    private:
        OptixDeviceContext context_ = nullptr;
        thesis::cuda::UploadBuffer<std::byte> output_buffer_;
        OptixTraversableHandle gas_handle_ = 0;
};

class TriangleGAS {
    public:
        TriangleGAS(OptixDeviceContext context, std::span<const float3> vertices)
            : d_vertices_(vertices.data(), vertices.size()),
              gas_(context, createBuildInput())
        {}

        [[nodiscard]] const OptixTraversableHandle& get() const noexcept { return gas_.get(); }
        [[nodiscard]] OptixTraversableHandle& get() noexcept { return gas_.get(); }
    
    private:
        OptixBuildInput createBuildInput() const {
            OptixBuildInput build_input = {};
            build_input.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
            build_input.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
            build_input.triangleArray.numVertices = static_cast<uint32_t>(d_vertices_.size());
            build_input.triangleArray.vertexBuffers = &d_vertices_.get();
            static constexpr uint32_t input_flags[1] = { OPTIX_GEOMETRY_FLAG_NONE };
            build_input.triangleArray.flags = input_flags;
            build_input.triangleArray.numSbtRecords = 1;
            return build_input;
        }
    
        thesis::cuda::UploadBuffer<float3> d_vertices_;
        thesis::optix::GASHandle gas_;
    };

}  // namespace thesis::optix