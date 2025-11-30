#pragma once

#include "thesis/common/utils/types.h"
#include "thesis/host/cuda/async_buffer.h"
#include "thesis/host/optix/acceleration_structure.h"

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

// TODO: there was a time when I tried storing the single vertex and radius together in one float4
// buffer of size 1 but I couldn't get that to work. Perhaps it can be done though
class SphereGAS {
    cuda::AsyncBuffer<float3> vertices_;  // Sphere centers
    cuda::AsyncBuffer<float> radii_;      // Sphere radii
    GAS gas_;

   public:
    SphereGAS(CUcontext ctx, std::shared_ptr<cuda::Stream> stream)
        : vertices_(1, ctx, stream, cuda::AllocType::OnBoth),
          radii_(1, ctx, stream, cuda::AllocType::OnBoth),
          gas_(ctx, std::move(stream)) {}

    SphereGAS(SphereGAS&&) noexcept = default;
    SphereGAS& operator=(SphereGAS&&) noexcept = default;

    void build(CUcontext cuda_ctx, OptixDeviceContext optix_ctx) {
        // Unit sphere at origin
        vertices_.host()[0] = make_float3(0.0f);
        vertices_.upload();

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

        gas_.build(in, cuda_ctx, optix_ctx);
        spdlog::debug("Sphere GAS built");
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return gas_.get(); }
};

}  // namespace thesis::host::optix
