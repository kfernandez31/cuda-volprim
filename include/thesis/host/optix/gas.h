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
    
        in.sphereArray.flags = geomFlags;
        in.sphereArray.numSbtRecords = 1;
        in.sphereArray.primitiveIndexOffset = 0;  // Explicitly set primitive index offset

        gas_.build(in, cuda_ctx, optix_ctx);
        spdlog::info("Sphere GAS built, handle = 0x{:x}", gas_.get());
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return gas_.get(); }
};

}  // namespace thesis::host::optix
