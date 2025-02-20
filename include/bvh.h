#pragma once

#include "mesh.h"

#define TINYBVH_IMPLEMENTATION
#include "tiny_bvh.h"

using BvhTriangle = tinybvh::bvhvec4;

struct BVH : public Object {
private:
	std::vector<std::shared_ptr<Object>> primitives;
	std::vector<tinybvh::bvhvec4> triangles;
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

		triangles.clear();
		triangles.reserve(primitives.size() * NUM_TRIANGLES_PER_PRIMITIVE);

		for (const auto& prim : primitives) {
			auto tris = prim->transform<tinybvh::bvhvec4>(BaseIcosphere);
			triangles.insert(triangles.end(), tris.begin(), tris.end());
		}

		bvh.Build(triangles.data(), triangles.size());
	}

    std::optional<HitRecord> intersect(const Ray& r_global) override {
		tinybvh::bvhvec3 origin(r_global.origin.x, r_global.origin.y, r_global.origin.z);
		tinybvh::bvhvec3 direction(r_global.direction.x, r_global.direction.y, r_global.direction.z);
		tinybvh::Ray r(origin, direction);

		bvh.Intersect(r);
		if (!std::isfinite(r.hit.t))
			return {};

		auto vert_idx = r.hit.prim;
		auto tri_idx  = vert_idx / 3;
		auto prim_idx = tri_idx / NUM_TRIANGLES_PER_PRIMITIVE;

		return primitives[prim_idx]->intersect(r_global);
	}
};
