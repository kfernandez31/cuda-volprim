#pragma once

#include "thesis/common/utils/types.h"
#include "thesis/host/cuda/async_buffer.h"
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

template <class Shape>
class TriangleGAS {
    static_assert(std::is_same_v<decltype(Shape::NumVertices), const size_t>);
    static_assert(std::is_same_v<decltype(Shape::NumIndices), const size_t>);

    cuda::AsyncBuffer<float3> vertices_;
    cuda::AsyncBuffer<uint3> indices_;
    GAS gas_;

   public:
    TriangleGAS(const Shape& shape, CUcontext ctx, std::shared_ptr<cuda::Stream> stream)
        : vertices_(Shape::NumVertices, ctx, stream, cuda::AllocType::OnBoth),
          indices_(Shape::NumIndices, ctx, stream, cuda::AllocType::OnBoth),
          gas_(ctx, std::move(stream)) {
        // Copy vertices
        const auto v_src = utils::data::reinterpretSpan<const float3>(shape.getVertices());
        std::memcpy(vertices_.host(), v_src.data(), v_src.size_bytes());
        vertices_.upload();

        // Copy indices
        const auto i_src = utils::data::reinterpretSpan<const uint3>(shape.getIndices());
        std::memcpy(indices_.host(), i_src.data(), i_src.size_bytes());
        indices_.upload();
    }

    TriangleGAS(TriangleGAS&&) noexcept = default;
    TriangleGAS& operator=(TriangleGAS&&) noexcept = default;

    void build(CUcontext cuda_ctx, OptixDeviceContext optix_ctx) {
        OptixBuildInput in{};
        in.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;

        auto vbuf = vertices_.cu_device_ptr();
        in.triangleArray.vertexBuffers = &vbuf;
        in.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
        in.triangleArray.numVertices = static_cast<uint>(vertices_.size());

        auto ibuf = indices_.cu_device_ptr();
        in.triangleArray.indexBuffer = ibuf;
        in.triangleArray.indexFormat = OPTIX_INDICES_FORMAT_UNSIGNED_INT3;
        in.triangleArray.numIndexTriplets = static_cast<uint>(indices_.size());

        static constexpr uint flags[1] = {OPTIX_GEOMETRY_FLAG_NONE};
        in.triangleArray.flags = flags;
        in.triangleArray.numSbtRecords = 1;

        gas_.build(in, cuda_ctx, optix_ctx);
        spdlog::info("Triangle GAS built, handle = 0x{:x}", gas_.get());
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return gas_.get(); }
};

}  // namespace thesis::host::optix
