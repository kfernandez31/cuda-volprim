#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/utils/utility.h"

namespace thesis {
namespace device {
namespace utils {

struct NullOptTag {
    explicit constexpr NullOptTag() = default;
};

constexpr NullOptTag nullopt{};

template <typename T>
struct Optional {
    bool has_value_;
    T value_;

    Optional() : has_value_(false) {}
    Optional(NullOptTag) : has_value_(false) {}
    Optional(const T& v) : has_value_(true), value_(v) {}

    Optional(const Optional&) = default;
    Optional& operator=(const Optional&) = default;

    Optional& operator=(NullOptTag) {
        has_value_ = false;
        return *this;
    }

    Optional& operator=(const T& v) {
        has_value_ = true;
        value_ = v;
        return *this;
    }

    template <typename... Args>
    __device__ void emplace(Args&&... args) {
        has_value_ = true;
        value_ = T(utility::forward<Args>(args)...);
    }

    __device__ void reset() { has_value_ = false; }

    __device__ bool has() const { return has_value_; }
    __device__ operator bool() const { return has_value_; }

    __device__ T& unwrap() { return value_; }
    __device__ const T& unwrap() const { return value_; }

    __device__ const T& operator*() const { return value_; }
    __device__ T& operator*() { return value_; }
};

template <typename T, typename... Args>
__device__ Optional<T> make_optional(Args&&... args) {
    return Optional<T>(utility::forward<Args>(args)...);
}

}  // namespace utils
}  // namespace device
}  // namespace thesis
