#pragma once

#include "thesis/host/geometry/mesh.h"
#include "thesis/host/optix/acceleration_structure.h"
#include "thesis/host/utils/data.h"

#include <vector_types.h>

#include <span>

namespace thesis::host::optix {

inline constexpr uint GAS_BUILD_FLAGS = OPTIX_BUILD_FLAG_ALLOW_COMPACTION |
                                        OPTIX_BUILD_FLAG_PREFER_FAST_TRACE |
                                        OPTIX_BUILD_FLAG_ALLOW_RANDOM_VERTEX_ACCESS;

class GAS : public AccelerationStructure {
   public:
    using AccelerationStructure::AccelerationStructure;

    void build(const OptixBuildInput& input, CUcontext cuda_ctx, OptixDeviceContext optix_ctx) {
        build_internal(input, cuda_ctx, optix_ctx, GAS_BUILD_FLAGS, "GAS");
    }
};

class SphereGAS {
    cuda::Buffer<float4> aabbs_;
    GAS gas_;

   public:
    SphereGAS(CUcontext ctx, std::shared_ptr<cuda::Stream> stream)
        : aabbs_(2, ctx, cuda::AllocType::OnBoth), gas_(ctx, std::move(stream)) {}

    void build(CUcontext cuda_ctx, OptixDeviceContext optix_ctx) {
        auto min = make_float3(-1.0f);
        auto max = make_float3(1.0f);

        aabbs_.host()[0] = make_float4(min.x, min.y, min.z, 0.0f);
        aabbs_.host()[1] = make_float4(max.x, max.y, max.z, 0.0f);
        aabbs_.upload();

        static constexpr unsigned int geomFlags[1] = {OPTIX_GEOMETRY_FLAG_NONE};

        OptixBuildInput in{};
        in.type = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;

        auto aabb_buffer = aabbs_.cu_device_ptr();

        in.customPrimitiveArray.aabbBuffers = &aabb_buffer;
        in.customPrimitiveArray.numPrimitives = 1;
        in.customPrimitiveArray.strideInBytes = sizeof(float4) * 2;

        in.customPrimitiveArray.flags = geomFlags;
        in.customPrimitiveArray.numSbtRecords = 1;
        in.customPrimitiveArray.sbtIndexOffsetBuffer = 0;
        in.customPrimitiveArray.sbtIndexOffsetSizeInBytes = 0;
        in.customPrimitiveArray.sbtIndexOffsetStrideInBytes = 0;
        in.customPrimitiveArray.primitiveIndexOffset = 0;

        gas_.build(in, cuda_ctx, optix_ctx);
        spdlog::info("Custom Sphere GAS built, handle = 0x{:x}", gas_.get());
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return gas_.get(); }
};

}  // namespace thesis::host::optix
