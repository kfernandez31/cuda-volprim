#pragma once

#include "math.h"

#include <glm/common.hpp>

template <typename T>
class interval_impl {
public:
    T min, max;

    interval_impl() : min(empty.min), max(empty.max) {}
    interval_impl(T min, T max) : min(std::min(min, max)), max(glm::max(min, max)) {}

    inline size_t size() const {
        return max - min + 1;
    }

    inline T clamp(T x) const {
        return glm::clamp(x, min, max);
    }

    inline bool contains(T x) const {
        return min <= x && x <= max;
    }

    inline bool surrounds(T x) const {
        return min < x && x < max;
    }

    interval_impl operator-() const {
        return interval_impl(-max, -min);
    }

    bool operator<(T x) const {
        return max < x;
    }

    bool operator<=(T x) const {
        return max <= x;
    }

    bool operator>(T x) const {
        return min > x;
    }

    bool operator>=(T x) const {
        return min >= x;
    }

    bool operator==(T x) const {
        return min == x && max == x;
    }

    static const interval_impl empty;
    static const interval_impl universe;
};

template <typename T>
const interval_impl<T> interval_impl<T>::universe(-math::inf<T>(), math::inf<T>());

template <typename T>
const interval_impl<T> interval_impl<T>::empty = -universe;

using Interval = interval_impl<float>;
