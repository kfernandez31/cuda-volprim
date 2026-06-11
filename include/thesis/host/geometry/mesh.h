#pragma once

// Triangle-mesh geometry for the tessellated-icosphere GAS (Chapter 6 G8: analytic
// built-in sphere vs. tessellated icosphere A/B). Only pulled in under THESIS_ICOSPHERE
// (via host/optix/gas.h). Resurrected from commit eb5372f and rewritten to use the
// project's float3 math (common/utils/math.h) instead of glm, which the current tree
// no longer depends on.

#include "thesis/common/utils/math.h"

#include <vector_types.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace thesis::host::geometry {

namespace cmath = ::thesis::common::math;

class Mesh {
   protected:
    std::vector<float3> vertices_;
    std::vector<uint3> indices_;

   public:
    Mesh() = default;

    Mesh(Mesh&&) noexcept = default;
    Mesh& operator=(Mesh&&) noexcept = default;
    Mesh(const Mesh&) = default;
    Mesh& operator=(const Mesh&) = default;

    Mesh(std::vector<float3>&& verts, std::vector<uint3>&& inds)
        : vertices_(std::move(verts)), indices_(std::move(inds)) {}

    [[nodiscard]] std::span<const float3> getVertices() const noexcept { return vertices_; }
    [[nodiscard]] std::span<const uint3> getIndices() const noexcept { return indices_; }
};

// Unit icosphere obtained by N rounds of 4:1 Loop-style subdivision of a regular
// icosahedron, with every vertex projected back onto the unit sphere. Vertices lie
// exactly on the unit sphere; triangle faces are inscribed chords (so the tessellated
// surface sits just inside the true sphere — the source of the faceting bias the G8
// experiment measures). Winding is CCW as seen from outside (outward normals), which
// the any-hit front-face filter relies on to keep the entry face.
template <std::size_t N>
struct Icosphere : public Mesh {
    static constexpr std::size_t NumVertices = 10 * cmath::pow<std::size_t>(4, N) + 2;
    static constexpr std::size_t NumIndices = 20 * cmath::pow<std::size_t>(4, N);

    Icosphere() {
        const float t = 0.5f * (1.0f + std::sqrt(5.0f));  // golden ratio

        vertices_ = {
            make_float3(-1,  t,  0), make_float3( 1,  t,  0), make_float3(-1, -t,  0),
            make_float3( 1, -t,  0), make_float3( 0, -1,  t), make_float3( 0,  1,  t),
            make_float3( 0, -1, -t), make_float3( 0,  1, -t), make_float3( t,  0, -1),
            make_float3( t,  0,  1), make_float3(-t,  0, -1), make_float3(-t,  0,  1),
        };
        indices_ = {
            make_uint3(0, 11, 5), make_uint3(0,  5,  1), make_uint3( 0,  1,  7),
            make_uint3(0,  7, 10), make_uint3(0, 10, 11), make_uint3( 1,  5,  9),
            make_uint3(5, 11,  4), make_uint3(11, 10, 2), make_uint3(10,  7,  6),
            make_uint3(7,  1,  8), make_uint3(3,  9,  4), make_uint3( 3,  4,  2),
            make_uint3(3,  2,  6), make_uint3(3,  6,  8), make_uint3( 3,  8,  9),
            make_uint3(4,  9,  5), make_uint3(2,  4, 11), make_uint3( 6,  2, 10),
            make_uint3(8,  6,  7), make_uint3(9,  8,  1),
        };

        for (auto& v : vertices_) v = cmath::normalize(v);

        if constexpr (N == 0) return;

        vertices_.reserve(NumVertices);
        indices_.reserve(NumIndices);

        std::unordered_map<std::uint64_t, std::uint32_t> cache;
        auto midpoint = [&](std::uint32_t a, std::uint32_t b) -> std::uint32_t {
            if (a > b) std::swap(a, b);
            const std::uint64_t key = (static_cast<std::uint64_t>(a) << 32) | b;
            if (auto it = cache.find(key); it != cache.end()) return it->second;
            vertices_.push_back(cmath::normalize(0.5f * (vertices_[a] + vertices_[b])));
            return cache[key] = static_cast<std::uint32_t>(vertices_.size() - 1);
        };

        std::vector<uint3> next;
        next.reserve(NumIndices);
        for (std::size_t round = 0; round < N; ++round) {
            next.clear();
            for (const auto& tri : indices_) {
                const std::uint32_t d = midpoint(tri.x, tri.y);
                const std::uint32_t e = midpoint(tri.y, tri.z);
                const std::uint32_t f = midpoint(tri.z, tri.x);
                next.push_back(make_uint3(tri.x, d, f));
                next.push_back(make_uint3(d, tri.y, e));
                next.push_back(make_uint3(e, tri.z, f));
                next.push_back(make_uint3(d, e, f));
            }
            indices_.swap(next);
        }
    }
};

}  // namespace thesis::host::geometry
