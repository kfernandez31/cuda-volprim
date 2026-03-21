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

// Interleaved float4 buffer: {x, y, z, radius} for compact storage
class SphereGAS {
    cuda::AsyncBuffer<float4> sphere_data_;  // Interleaved center (xyz) + radius (w)
    GAS gas_;

   public:
    SphereGAS(CUcontext ctx, std::shared_ptr<cuda::Stream> stream)
        : sphere_data_(1, ctx, stream, cuda::AllocType::OnBoth),
          gas_(ctx, std::move(stream)) {}

    SphereGAS(SphereGAS&&) noexcept = default;
    SphereGAS& operator=(SphereGAS&&) noexcept = default;

    void build(CUcontext cuda_ctx, OptixDeviceContext optix_ctx) {
        // Unit sphere at origin: center=(0,0,0), radius=1
        sphere_data_.host()[0] = make_float4(0.0f, 0.0f, 0.0f, 1.0f);
        sphere_data_.upload();

        static constexpr uint geomFlags[1] = {OPTIX_GEOMETRY_FLAG_NONE};

        OptixBuildInput in{};
        in.type = OPTIX_BUILD_INPUT_TYPE_SPHERES;

        // Setup interleaved buffer: vertex at offset 0, radius at offset 12 bytes
        const CUdeviceptr base_ptr = sphere_data_.cu_device_ptr();
        const CUdeviceptr vertex_buffer_ptr = base_ptr;
        const CUdeviceptr radius_buffer_ptr = base_ptr + 3 * sizeof(float);

        in.sphereArray.vertexBuffers = &vertex_buffer_ptr;
        in.sphereArray.vertexStrideInBytes = sizeof(float4);
        in.sphereArray.numVertices = 1;

        in.sphereArray.radiusBuffers = &radius_buffer_ptr;
        in.sphereArray.radiusStrideInBytes = sizeof(float4);
        in.sphereArray.singleRadius = 0;

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
