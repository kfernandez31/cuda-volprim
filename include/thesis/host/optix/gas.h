#pragma once

#include "thesis/host/optix/acceleration_structure.h"
#include "thesis/host/geometry/mesh.h"
#include "thesis/host/utils/data.h"

#include <vector_types.h>
#include <span>

namespace thesis::host::optix {

inline constexpr uint GAS_BUILD_FLAGS =
    OPTIX_BUILD_FLAG_ALLOW_COMPACTION | OPTIX_BUILD_FLAG_PREFER_FAST_TRACE | OPTIX_BUILD_FLAG_ALLOW_RANDOM_VERTEX_ACCESS;

class GAS : public AccelerationStructure {
   public:
    using AccelerationStructure::AccelerationStructure;

    void build(const OptixBuildInput& input, CUcontext cuda_ctx, OptixDeviceContext optix_ctx) {
        build_internal(input, cuda_ctx, optix_ctx, GAS_BUILD_FLAGS, "GAS");
    }
};

class SphereGAS {
    cuda::Buffer<float3> vertices_;  // Separate buffer for vertices
    cuda::Buffer<float> radii_;      // Separate buffer for radii
    GAS gas_;

   public:
    SphereGAS(CUcontext ctx, std::shared_ptr<cuda::Stream> stream)
        : vertices_(1, ctx, cuda::AllocType::OnBoth)
        , radii_(1, ctx, cuda::AllocType::OnBoth)
        , gas_(ctx, std::move(stream)) {}

    void build(CUcontext cuda_ctx, OptixDeviceContext optix_ctx) {
        // Set sphere center at origin
        vertices_.host()[0] = make_float3(0.0f, 0.0f, 0.0f);
        vertices_.upload();
        
        // Set sphere radius to 1.0
        radii_.host()[0] = 1.0f;
        radii_.upload();

        static constexpr uint geomFlags[1] = {OPTIX_GEOMETRY_FLAG_NONE};

        OptixBuildInput in{};
        in.type = OPTIX_BUILD_INPUT_TYPE_SPHERES;

        CUdeviceptr vertex_buffer_ptr = vertices_.cu_device_ptr();
        CUdeviceptr radius_buffer_ptr = radii_.cu_device_ptr();
        
        in.sphereArray.vertexBuffers = &vertex_buffer_ptr;
        in.sphereArray.vertexStrideInBytes = sizeof(float3);
        in.sphereArray.numVertices = 1;
        
        in.sphereArray.radiusBuffers = &radius_buffer_ptr;
        in.sphereArray.radiusStrideInBytes = sizeof(float);
        in.sphereArray.singleRadius = false;
    
        in.sphereArray.flags = geomFlags;
        in.sphereArray.numSbtRecords = 1;
        in.sphereArray.sbtIndexOffsetBuffer = 0;
        in.sphereArray.sbtIndexOffsetSizeInBytes = 0;
        in.sphereArray.sbtIndexOffsetStrideInBytes = 0;
        in.sphereArray.primitiveIndexOffset = 0;

        spdlog::info("Sphere buffer ptrs: vertex=0x{:x}, radius=0x{:x}",
                    vertex_buffer_ptr, radius_buffer_ptr);
        spdlog::info("Sphere center: (0,0,0), radius: 1.0");

        gas_.build(in, cuda_ctx, optix_ctx);
        spdlog::info("Sphere GAS built, handle = 0x{:x}", gas_.get());
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return gas_.get(); }
};

}  // namespace thesis::host::optix
