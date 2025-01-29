#pragma once

#include "object.h"

#include <memory>
#include <vector>

class ObjectList : public Object {
public:
    std::vector<std::shared_ptr<Object>> objects;

    ObjectList(std::initializer_list<std::shared_ptr<Object>> objs) : objects(objs) {}

    template <typename... Objs>
    ObjectList(Objs... objs) : Object(), objects{objs...} {}

    std::optional<HitRecord> intersect(const Ray& r, const Interval& t_range) override {
        auto cur_interval = t_range;
        std::optional<HitRecord> result;

        for (const auto& obj : objects) {
            auto hit = obj->intersect(r, cur_interval);
            auto t_in = hit->t_in;
            if (hit && t_in < cur_interval.max) {
                cur_interval.max = t_in;
                result = hit;
            }
        }

        return result;
    }
};
