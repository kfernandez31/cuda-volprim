#pragma once

#include "thesis/common/utils/types.h"
#include "thesis/host/cuda/async_buffer.h"
#include "thesis/host/optix/acceleration_structure.h"

#include <vector_types.h>

#include <span>

#ifdef THESIS_ICOSPHERE
#include "thesis/host/geometry/mesh.h"

#include <cstring>
#ifndef THESIS_ICOSPHERE_N
#define THESIS_ICOSPHERE_N 3
#endif
#endif

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
        CUdeviceptr base_ptr = sphere_data_.cu_device_ptr();
        CUdeviceptr vertex_buffer_ptr = base_ptr;
        CUdeviceptr radius_buffer_ptr = base_ptr + 3 * sizeof(float);

        in.sphereArray.vertexBuffers = &vertex_buffer_ptr;
        in.sphereArray.vertexStrideInBytes = sizeof(float4);
        in.sphereArray.numVertices = 1;

        in.sphereArray.radiusBuffers = &radius_buffer_ptr;
        in.sphereArray.radiusStrideInBytes = sizeof(float4);
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

#ifdef THESIS_ICOSPHERE
// Tessellated-icosphere GAS — the drop-in alternative to SphereGAS for the Chapter 6
// analytic-vs-tessellated A/B (G8). Exposes the SAME (ctx, stream) construction and
// build()/get() interface as SphereGAS, so renderer.cpp swaps the two via a single
// typedef under the THESIS_ICOSPHERE guard; the IAS instancing, any-hit entry
// collection and analytic optical-depth integration are all unchanged. The unit
// icosphere is mapped to each Gaussian's 3σ ellipsoid by the same per-instance
// localToWorld transform the analytic unit sphere uses. Subdivision level N is fixed at
// compile time (THESIS_ICOSPHERE_N): N=0 → 20 tris / 12 verts … N=3 → 1280 tris / 642
// verts. The built-in triangle intersector is used (no custom IS program); a ray crosses
// each convex icosphere through a front (entry) and a back (exit) face, and the any-hit
// keeps only the entry face (see device/entry/anyhit.cuh) so each primitive is collected
// once, exactly as the single-hit built-in sphere is.
class IcosphereGAS {
    using Shape = geometry::Icosphere<THESIS_ICOSPHERE_N>;

    cuda::AsyncBuffer<float3> vertices_;
    cuda::AsyncBuffer<uint3> indices_;
    GAS gas_;

   public:
    IcosphereGAS(CUcontext ctx, std::shared_ptr<cuda::Stream> stream)
        : vertices_(Shape::NumVertices, ctx, stream, cuda::AllocType::OnBoth),
          indices_(Shape::NumIndices, ctx, stream, cuda::AllocType::OnBoth),
          gas_(ctx, std::move(stream)) {}

    IcosphereGAS(IcosphereGAS&&) noexcept = default;
    IcosphereGAS& operator=(IcosphereGAS&&) noexcept = default;

    void build(CUcontext cuda_ctx, OptixDeviceContext optix_ctx) {
        const Shape shape;  // host-built unit icosphere (vertices on the unit sphere)
        const auto verts = shape.getVertices();
        const auto inds = shape.getIndices();
        std::memcpy(vertices_.host(), verts.data(), verts.size_bytes());
        vertices_.upload();
        std::memcpy(indices_.host(), inds.data(), inds.size_bytes());
        indices_.upload();

        static constexpr uint geomFlags[1] = {OPTIX_GEOMETRY_FLAG_NONE};

        OptixBuildInput in{};
        in.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;

        CUdeviceptr vbuf = vertices_.cu_device_ptr();
        in.triangleArray.vertexBuffers = &vbuf;
        in.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
        in.triangleArray.vertexStrideInBytes = sizeof(float3);
        in.triangleArray.numVertices = static_cast<uint>(vertices_.size());

        in.triangleArray.indexBuffer = indices_.cu_device_ptr();
        in.triangleArray.indexFormat = OPTIX_INDICES_FORMAT_UNSIGNED_INT3;
        in.triangleArray.indexStrideInBytes = sizeof(uint3);
        in.triangleArray.numIndexTriplets = static_cast<uint>(indices_.size());

        in.triangleArray.flags = geomFlags;
        in.triangleArray.numSbtRecords = 1;

        gas_.build(in, cuda_ctx, optix_ctx);
        spdlog::info("Icosphere GAS built (N={}, {} verts, {} tris)", THESIS_ICOSPHERE_N,
                     vertices_.size(), indices_.size());
    }

    [[nodiscard]] OptixTraversableHandle get() const noexcept { return gas_.get(); }
};
#endif  // THESIS_ICOSPHERE

}  // namespace thesis::host::optix
