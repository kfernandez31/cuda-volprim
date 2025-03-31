#pragma once

#include "math.h"

#include <glm/common.hpp>
#include <glm/gtc/epsilon.hpp>

template <typename T>
class interval_impl {
public:
    T min, max;

    interval_impl()
        : min(empty.min), max(empty.max) {}

    interval_impl(T min, T max)
        : min(glm::min(min, max)), max(glm::max(min, max)) {}

    interval_impl(const interval_impl& a, const interval_impl& b)
        : min(glm::min(a.min, b.min)), max(glm::max(a.max, b.max)) {}

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

    interval_impl expand(T delta) const {
        auto padding = delta / 2;
        return interval_impl(min - padding, max + padding);
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

    std::ostream& operator<<(std::ostream& os) {
        os << "[" << min << ", " << max << "]";
        return os;
    }

    bool operator==(const interval_impl<T>& other) const {
        constexpr T eps(1e-8);
        return glm::epsilonEqual(min, other.min, eps) && glm::epsilonEqual(max, other.max, eps);
    }

    bool operator==(T x) const {
        return this == interval_impl<T>(x, x);
    }

    static const interval_impl empty;
    static const interval_impl universe;
    static const interval_impl ahead;
};

template <typename T>
const interval_impl<T> interval_impl<T>::ahead(0, math::inf<T>());

template <typename T>
const interval_impl<T> interval_impl<T>::universe(-math::inf<T>(), math::inf<T>());

template <typename T>
const interval_impl<T> interval_impl<T>::empty = -universe;

using Interval = interval_impl<float>;
