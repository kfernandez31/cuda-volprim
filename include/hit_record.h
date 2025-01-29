#pragma once

#include <memory>

class Object;

class HitRecord {
public:
    std::shared_ptr<Object> object;
    float t_in;
    float t_out;

    HitRecord(std::shared_ptr<Object> _object, float _t_in, float _t_out)
        : object(_object)
        , t_in(_t_in)
        , t_out(_t_out)
    {}

    HitRecord() {}
};
