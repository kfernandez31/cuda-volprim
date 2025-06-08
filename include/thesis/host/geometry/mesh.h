#pragma once

#include "thesis/common/utils/math.h"

#include <cstddef>
#include <unordered_map>
#include <utility>
#include <vector>

namespace thesis::host::geometry {

class Mesh {
   protected:
    std::vector<glm::vec3> vertices_;
    std::vector<glm::uvec3> indices_;

   public:
    Mesh() = default;

    Mesh(Mesh&&) noexcept = default;
    Mesh& operator=(Mesh&&) noexcept = default;

    Mesh(const Mesh&) = default;
    Mesh& operator=(const Mesh&) = default;

    Mesh(const std::vector<glm::vec3>& _vertices, const std::vector<glm::uvec3>& _indices)
        : vertices_(_vertices), indices_(_indices) {}

    Mesh(std::span<const glm::vec3> verts, std::span<const glm::uvec3> inds)
        : vertices_(verts.begin(), verts.end()), indices_(inds.begin(), inds.end()) {}

    Mesh(std::vector<glm::vec3>&& verts, std::vector<glm::uvec3>&& inds)
        : vertices_(std::move(verts)), indices_(std::move(inds)) {}

    Mesh(std::initializer_list<glm::vec3> verts, std::initializer_list<glm::uvec3> inds)
        : vertices_(verts), indices_(inds) {}

    [[nodiscard]] std::span<const glm::vec3> getVertices() const noexcept { return vertices_; }

    [[nodiscard]] std::span<const glm::uvec3> getIndices() const noexcept { return indices_; }

    void transform(const glm::mat4& transformation_matrix) noexcept {
        std::transform(vertices_.begin(), vertices_.end(), vertices_.begin(),
                       [&](auto v) { return transformation_matrix * glm::vec4(v, 1.0f); });
    }

    void offsetIndices(uint offset) noexcept {
        std::transform(indices_.begin(), indices_.end(), indices_.begin(),
                       [&](auto i) { return i + offset; });
    }
};

template <size_t N>
struct Icosphere : public Mesh {
    static constexpr size_t NumVertices = 10 * common::math::pow<size_t>(4, N) + 2;
    static constexpr size_t NumIndices = 20 * common::math::pow<size_t>(4, N);

    explicit Icosphere(float t)
        : Mesh(
              // clang-format off
              {
                {-1,  t,  0}, { 1,  t,  0}, {-1, -t,  0}, { 1, -t,  0},
                { 0, -1,  t}, { 0,  1,  t}, { 0, -1, -t}, { 0,  1, -t},
                { t,  0, -1}, { t,  0,  1}, {-t,  0, -1}, {-t,  0,  1},
              },
              // clang-format off
              {
                {0, 11, 5}, {0,  5,  1}, { 0,  1,  7}, { 0, 7, 10}, {0, 10, 11},
                {1,  5, 9}, {5, 11,  4}, {11, 10,  2}, {10, 7,  6}, {7,  1,  8},
                {3,  9, 4}, {3,  4,  2}, { 3,  2,  6}, { 3, 6,  8}, {3,  8,  9},
                {4,  9, 5}, {2,  4, 11}, { 6,  2, 10}, { 8, 6,  7}, {9,  8,  1},
              }) {
        for (auto& v : vertices_) {
            v = glm::normalize(v);
        }

        if constexpr (N == 0) {
            return;
        }

        // Prealloc since we know the space bounds
        vertices_.reserve(NumVertices);
        indices_.reserve(NumIndices);

        std::unordered_map<uint64_t, uint32_t> cache;
        auto getMidPoint = [&](uint32_t v1, uint32_t v2) mutable -> uint32_t {
            if (v1 > v2) {
                std::swap(v1, v2);
            }

            uint64_t key = (static_cast<uint64_t>(v1) << 32) | v2;
            if (auto it = cache.find(key); it != cache.end()) {
                return it->second;
            }

            auto middle = glm::normalize(0.5f * (vertices_[v1] + vertices_[v2]));
            vertices_.push_back(middle);

            return cache[key] = static_cast<uint32_t>(vertices_.size() - 1);
        };

        std::vector<glm::uvec3> temp_indices;
        temp_indices.reserve(NumIndices);

        for (size_t i = 0; i < N; ++i) {
            temp_indices.clear();

            for (const auto& tri : indices_) {
                auto d = getMidPoint(tri[0], tri[1]);
                auto e = getMidPoint(tri[1], tri[2]);
                auto f = getMidPoint(tri[2], tri[0]);

                temp_indices.insert(temp_indices.end(),
                                    {{tri[0], d, f}, {d, tri[1], e}, {f, e, tri[2]}, {d, e, f}});
            }

            indices_.swap(temp_indices);
        }
    }

    Icosphere() : Icosphere(0.5f * (1.0f + glm::sqrt(5.0f))) {}

    Icosphere(Icosphere&&) noexcept = default;
    Icosphere& operator=(Icosphere&&) noexcept = default;

    Icosphere(const Icosphere&) = default;
    Icosphere& operator=(const Icosphere&) = default;
};

static const Icosphere<0> BaseIcosphere;

}  // namespace thesis::host::geometry
