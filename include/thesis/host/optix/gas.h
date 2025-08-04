#pragma once

#include "thesis/host/optix/acceleration_structure.h"
#include "thesis/host/geometry/mesh.h"
#include "thesis/host/utils/data.h"

#include <vector_types.h>
#include <span>

namespace thesis::host::optix {

inline constexpr uint GAS_BUILD_FLAGS =
    OPTIX_BUILD_FLAG_ALLOW_COMPACTION | OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;

class GAS : public AccelerationStructure {
   public:
    using AccelerationStructure::AccelerationStructure;

    void build(const OptixBuildInput& input, CUcontext cuda_ctx, OptixDeviceContext optix_ctx) {
        build_internal(input, cuda_ctx, optix_ctx, GAS_BUILD_FLAGS, "GAS");
    }
};

class SphereGAS {
    cuda::AsyncBuffer<float4> spheres_; // TODO(kacper): compact into one buffer
    GAS gas_;
    CUdeviceptr vertex_buffer_ptr_;
    CUdeviceptr radius_buffer_ptr_;

   public:
    SphereGAS(CUcontext ctx, std::shared_ptr<cuda::Stream> stream)
        : spheres_(1, ctx, stream, cuda::AllocType::OnBoth)
        , gas_(ctx, std::move(stream))
        , vertex_buffer_ptr_(spheres_.cu_device_ptr())
        , radius_buffer_ptr_(vertex_buffer_ptr_ + sizeof(float3)) {}

    void build(CUcontext cuda_ctx, OptixDeviceContext optix_ctx) {
        const auto sphere = make_float4(0.0f, 0.0f, 0.0f, 1.0f);
        spheres_.host()[0] = sphere;
        spheres_.upload();

        static constexpr uint geomFlags[1] = {OPTIX_GEOMETRY_FLAG_NONE};

        OptixBuildInput in{};
        in.type = OPTIX_BUILD_INPUT_TYPE_SPHERES;

        in.sphereArray.vertexBuffers = &vertex_buffer_ptr_;
        in.sphereArray.numVertices = 1;
        
        in.sphereArray.radiusBuffers = &radius_buffer_ptr_;
    
        in.sphereArray.flags = geomFlags;
        in.sphereArray.numSbtRecords = 1;

        spdlog::info("Sphere buffer ptrs: vertex=0x{:x}, radius=0x{:x}",
                    vertex_buffer_ptr_, radius_buffer_ptr_);

        gas_.build(in, cuda_ctx, optix_ctx);
        spdlog::info("Sphere GAS built, handle = 0x{:x}", gas_.get());
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return gas_.get(); }
};

}  // namespace thesis::host::optix
