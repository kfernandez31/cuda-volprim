#pragma once

#include "object.h"

#include <memory>
#include <vector>

class ObjectList : public Object {
public:
    std::vector<std::shared_ptr<Object>> objects;

    ObjectList(const std::vector<std::shared_ptr<Object>>& objs) : objects(objs) {}

    ObjectList(std::initializer_list<std::shared_ptr<Object>> objs) : objects(objs) {}

    // template <typename... Objs>
    // ObjectList(Objs... objs) : objects{objs...} {}

    std::optional<HitRecord> intersect(const Ray& r) override {
        std::optional<HitRecord> result;
        auto t_closest = math::inf<float>();

        for (const auto& obj : objects) {
            auto hit = obj->intersect(r);
            if (hit && hit->t_in < t_closest) {
                t_closest = hit->t_in;
                result = hit;
            }
        }

        return result;
    }
};
