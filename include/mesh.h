#pragma once

#include "mat.h"

#include <vector>
#include <unordered_map>

#include "tiny_bvh.h"

static constexpr size_t constexpr_pow(size_t base, size_t exp) {
    return (exp == 0) ? 1 : base * constexpr_pow(base, exp - 1);
}

struct Mesh {
    std::vector<vec3> vertices;
    std::vector<uvec3> triangles;

    Mesh() = default;

    Mesh(const std::vector<vec3>& _vertices, const std::vector<uvec3>& _triangles) : vertices(_vertices), triangles(_triangles) {}
};

struct Icosphere : public Mesh {
public:
    Icosphere(size_t num_subdivisions, float t = 0.5f * (1.0f + glm::sqrt(5.0f))) :
        Mesh({
            {-1,  t,  0}, { 1,  t,  0}, {-1, -t,  0}, { 1, -t,  0},
            { 0, -1,  t}, { 0,  1,  t}, { 0, -1, -t}, { 0,  1, -t},
            { t,  0, -1}, { t,  0,  1}, {-t,  0, -1}, {-t,  0,  1},
        },
        {
            {0, 11, 5}, {0,  5,  1}, { 0,  1,  7}, { 0, 7, 10}, {0, 10, 11},
            {1,  5, 9}, {5, 11,  4}, {11, 10,  2}, {10, 7,  6}, {7,  1,  8},
            {3,  9, 4}, {3,  4,  2}, { 3,  2,  6}, { 3, 6,  8}, {3,  8,  9},
            {4,  9, 5}, {2,  4, 11}, { 6,  2, 10}, { 8, 6,  7}, {9,  8,  1},
        })
    {
        for (auto& v : vertices)
            v = glm::normalize(v);

        if (num_subdivisions == 0)
            return;

        auto getMidPoint = [this, cache = std::unordered_map<uint64_t, uint32_t>()](uint32_t v1, uint32_t v2) mutable -> uint32_t {
            if (v1 > v2) std::swap(v1, v2);

            uint64_t key = ((uint64_t)v1 << 32) | v2;
            if (cache.count(key))
                return cache[key];

            auto middle = glm::normalize(0.5f * (vertices[v1] + vertices[v2]));
            vertices.push_back(middle);

            return cache[key] = vertices.size() - 1;
        };

        vertices.reserve((10 * (1 << (2 * num_subdivisions))) + 2); // TODO: verify
        for (size_t i = 0; i < num_subdivisions; ++i) {
            std::vector<uvec3> new_triangles;
            new_triangles.reserve(4 * triangles.size());

            for (const auto& tri : triangles) {
                auto d = getMidPoint(tri[0], tri[1]);
                auto e = getMidPoint(tri[1], tri[2]);
                auto f = getMidPoint(tri[2], tri[0]);

                new_triangles.insert(new_triangles.end(), {{tri[0], d, f}, {d, tri[1], e}, {f, e, tri[2]}, {d, e, f}});
            }

            triangles = std::move(new_triangles);
        }
    }
};

static constexpr size_t NUM_BASE_ICOSPHERE_SUBDIVISIONS = 0;
static constexpr size_t NUM_TRIANGLES_PER_PRIMITIVE     = 20 * (1 << (2 * NUM_BASE_ICOSPHERE_SUBDIVISIONS));
static const Icosphere BaseIcosphere(NUM_BASE_ICOSPHERE_SUBDIVISIONS); // TODO: could be constexpr
