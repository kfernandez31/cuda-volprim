#pragma once

#include "object.h"

#include <memory>
#include <vector>
#include <unordered_set>

class ObjectList { // TODO: inherit from Object
public:
    std::vector<std::shared_ptr<Object>> objects;

    ObjectList(const std::vector<std::shared_ptr<Object>>& objs) : objects(objs) {}

    ObjectList(std::initializer_list<std::shared_ptr<Object>> objs) : objects(objs) {}

    // template <typename... Objs>
    // ObjectList(Objs... objs) : objects{objs...} {}

    std::optional<HitRecord> intersect(const Ray& r, float t_min, const std::unordered_set<ObjectId>& excluded) {
        std::optional<HitRecord> result;
        auto t_closest = math::inf<float>();

        for (const auto& obj : objects) {
            if (excluded.find(obj->id) != excluded.end())
                continue;

            auto hit = obj->intersect(r, t_min);
            if (hit && hit->t_in < t_closest) {
                t_closest = hit->t_in;
                result = hit;
            }
        }

        return result;
    }
};
