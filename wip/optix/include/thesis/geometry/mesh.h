#pragma once

#include "thesis/utils/math.h"

#include <cstddef>
#include <unordered_map>
#include <utility>
#include <vector>

namespace thesis::geometry {

class Mesh {
   protected:
    std::vector<glm::vec3> vertices;
    std::vector<glm::uvec3> indices;

   public:
    Mesh() = default;
    Mesh(const Mesh&) noexcept = default;

    Mesh(const std::vector<glm::vec3>& _vertices, const std::vector<glm::uvec3>& _indices)
        : vertices(_vertices), indices(_indices) {}

    Mesh(std::span<const glm::vec3> verts, std::span<const glm::uvec3> inds)
        : vertices(verts.begin(), verts.end()), indices(inds.begin(), inds.end()) {}

    Mesh(std::vector<glm::vec3>&& verts, std::vector<glm::uvec3>&& inds)
        : vertices(std::move(verts)), indices(std::move(inds)) {}

    Mesh(std::initializer_list<glm::vec3> verts, std::initializer_list<glm::uvec3> inds)
        : vertices(verts), indices(inds) {}

    [[nodiscard]] std::span<const glm::vec3> getVertices() const noexcept { return vertices; }

    [[nodiscard]] std::span<const glm::uvec3> getIndices() const noexcept { return indices; }

    // TODO(kacper): remove
    void translate(glm::vec3 offset) noexcept {
        std::transform(vertices.begin(), vertices.end(), vertices.begin(),
                       [=](auto v) { return v + offset; });
    }

    // TODO(kacper): index offset param for ctor?

    void transform(const glm::mat4& transformation_matrix) noexcept {
        std::transform(vertices.begin(), vertices.end(), vertices.begin(),
                       [&](auto v) { return transformation_matrix * glm::vec4(v, 1.0f); });
    }

    // TODO(kacper): if stored as just a float matrix (4 x num_vertices), we could do gemm
    // but the sizes we want to use are probably not worth gpu transfers (and the 4x4 lhs is tiny)
    // [][][][] @ [][][]...[]
    // [][][][] @ [][][]...[]
    // [][][][] @ [][][]...[]
    // [][][][] @ [][][]...[]
};

/* TODO(kacper) - the class could be made constexpr if:
- we replace std::vector with std::array
- we replace the cache with a fixed-size, O(n) lookup one like so:
```
template <size_t Capacity>
struct MidpointCache {
    std::array<std::pair<uint64_t, uint32_t>, Capacity> entries = {};
    size_t count = 0;

    constexpr uint32_t getOrAdd(uint64_t key, auto add_fn) {
        for (size_t i = 0; i < count; ++i) {
            if (entries[i].first == key)
                return entries[i].second;
        }
        const uint32_t value = add_fn();
        entries[count++] = {key, value};
        return value;
    }
};
```
*/

template <size_t N>
struct Icosphere : public Mesh {
    static constexpr size_t NumVertices = 10 * math::constexpr_pow<size_t>(4, N) + 2;
    static constexpr size_t NumIndices = 20 * math::constexpr_pow<size_t>(4, N);

    explicit Icosphere(float t = 0.5f * (1.0f + glm::sqrt(5.0f)))
        : Mesh(
              {
                  {-1, t, 0},
                  {1, t, 0},
                  {-1, -t, 0},
                  {1, -t, 0},
                  {0, -1, t},
                  {0, 1, t},
                  {0, -1, -t},
                  {0, 1, -t},
                  {t, 0, -1},
                  {t, 0, 1},
                  {-t, 0, -1},
                  {-t, 0, 1},
              },
              {
                  {0, 11, 5}, {0, 5, 1},  {0, 1, 7},   {0, 7, 10}, {0, 10, 11},
                  {1, 5, 9},  {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
                  {3, 9, 4},  {3, 4, 2},  {3, 2, 6},   {3, 6, 8},  {3, 8, 9},
                  {4, 9, 5},  {2, 4, 11}, {6, 2, 10},  {8, 6, 7},  {9, 8, 1},
              }) {
        for (auto& v : vertices)
            v = glm::normalize(v);

        if constexpr (N == 0)
            return;

        // Prealloc since we know the space bounds
        vertices.reserve(NumVertices);
        indices.reserve(NumIndices);

        std::unordered_map<uint64_t, uint32_t> cache;
        auto getMidPoint = [&](uint32_t v1, uint32_t v2) mutable -> uint32_t {
            if (v1 > v2) {
                std::swap(v1, v2);
            }

            uint64_t key = (static_cast<uint64_t>(v1) << 32) | v2;
            if (auto it = cache.find(key); it != cache.end()) {
                return it->second;
            }

            auto middle = glm::normalize(0.5f * (vertices[v1] + vertices[v2]));
            vertices.push_back(middle);

            return cache[key] = static_cast<uint32_t>(vertices.size() - 1);
        };

        std::vector<glm::uvec3> temp_indices;
        temp_indices.reserve(NumIndices);

        for (size_t i = 0; i < N; ++i) {
            temp_indices.clear();

            for (const auto& tri : indices) {
                auto d = getMidPoint(tri[0], tri[1]);
                auto e = getMidPoint(tri[1], tri[2]);
                auto f = getMidPoint(tri[2], tri[0]);

                temp_indices.insert(temp_indices.end(),
                                    {{tri[0], d, f}, {d, tri[1], e}, {f, e, tri[2]}, {d, e, f}});
            }

            indices.swap(temp_indices);
        }
    }
};

static const Icosphere<0> BaseIcosphere;

}  // namespace thesis::geometry
