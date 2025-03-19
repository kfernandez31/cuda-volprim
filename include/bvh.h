#pragma once

#include "mesh.h"

#define TINYBVH_IMPLEMENTATION
#include "tiny_bvh.h"

using BvhTriangle = tinybvh::bvhvec4;

struct BVH : public Object {
private:
	std::vector<std::shared_ptr<Object>> primitives;
	std::vector<tinybvh::bvhvec4> vertices;
	tinybvh::BVH bvh;
public:
	BVH(const std::vector<std::shared_ptr<Object>>& prims) : primitives(prims) {
		rebuild();
	}

	BVH(std::initializer_list<std::shared_ptr<Object>> prims) : primitives(prims) {
		rebuild();
	}

	template <typename... Objs>
	BVH(Objs... prims) : primitives{prims...} {
		rebuild();
	}

	void rebuild() {
		if (primitives.empty())
			return;

		vertices.clear();
		vertices.reserve(primitives.size() * NUM_TRIANGLES_PER_PRIMITIVE);

		for (const auto& prim : primitives) {
			auto vs = prim->transform<tinybvh::bvhvec4>(BaseIcosphere);
			vertices.insert(vertices.end(), vs.begin(), vs.end());
		}

		bvh.Build(vertices.data(), vertices.size());
	}

    std::optional<HitRecord> intersect(const Ray& r_global, float t_min) override {
		tinybvh::bvhvec3 origin(r_global.origin.x, r_global.origin.y, r_global.origin.z);
		tinybvh::bvhvec3 direction(r_global.direction.x, r_global.direction.y, r_global.direction.z);
		tinybvh::Ray r(origin, direction);

		bvh.Intersect(r);
		if (!std::isfinite(r.hit.t))
			return {};

		// static std::vector<int> hit_cnt(vertices.size() / 3, 0);
		// static std::unordered_map<int, std::shared_ptr<Triangle>> tris;
		// static bool initialized = false;
		// if (!initialized) {
		// 	initialized = true;
		// 	for (size_t i = 0; i < vertices.size(); i += 3) {
		// 		vec3 u(vertices[i + 0].x, vertices[i + 0].y, vertices[i + 0].z);
		// 		vec3 v(vertices[i + 1].x, vertices[i + 1].y, vertices[i + 1].z);
		// 		vec3 w(vertices[i + 2].x, vertices[i + 2].y, vertices[i + 2].z);

		// 		auto tri = std::make_shared<Triangle>(u, v, w);
		// 		tri->albedo = random_vec<vec3>();
		// 		tris[i / 3] = tri;
		// 	}
		// }

		auto vert_idx = r.hit.prim;
		auto tri_idx  = vert_idx / 3;
		auto prim_idx = tri_idx / NUM_TRIANGLES_PER_PRIMITIVE;
		// dbg(vertices.size());
		// dbg(primitives.size());
		// dbg(NUM_TRIANGLES_PER_PRIMITIVE);

		// hit_cnt[tri_idx] += 1;
		// for (const auto& x : hit_cnt)
		// 	std::cout << x << ' ';
		// std::cout << std::endl;


		// if (tri_idx > 20)
		// 	dbg(tri_idx);
		// if (prim_idx == 0)
		// 	dbg(vert_idx, tri_idx, prim_idx);


		// return tris[tri_idx]->intersect(r_global);
		return primitives[prim_idx]->intersect(r_global, t_min);
	}
};
