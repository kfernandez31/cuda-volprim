#pragma once

#include <memory>

class Object;

class HitRecord {
public:
    std::shared_ptr<Object> object;
    float t_in, t_out;

    HitRecord(std::shared_ptr<Object> _object, float _t_in, float _t_out)
        : object(_object)
        , t_in(_t_in)
        , t_out(_t_out)
        {}

    // TODO: remove
    // HitRecord(std::shared_ptr<Object> _object, float _t_in, float _t_out, float _u, float _v)
    //     : HitRecord(_object, _t_in, _t_out)
    //     {}

    HitRecord() = default;
};
