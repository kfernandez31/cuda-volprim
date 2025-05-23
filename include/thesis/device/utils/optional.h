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

   __device__ Optional() : has_value_(false) {}
   __device__ Optional(NullOptTag) : has_value_(false) {}

    template <typename... Args>
    __device__ Optional(Args&&... args)
        : has_value_(true), value_(T(utility::forward<Args>(args)...)) {}

    __device__ Optional(Optional&& other) noexcept
        : has_value_(utility::exchange(other.has_value_, false)),
          value_(utility::exchange(other.value_, 0)) {}

    THESIS_INLINE THESIS_HOST_DEVICE Optional& operator=(Optional&& other) noexcept {
        has_value_ = utility::exchange(other.has_value_, false);
        value_ = utility::exchange(other.value_, 0);
        return *this;
    }

    Optional(const Optional&) = default;
    Optional& operator=(const Optional&) = default;

    THESIS_INLINE THESIS_HOST_DEVICE Optional& operator=(const T& v) noexcept {
        has_value_ = true;
        value_ = v;
        return *this;
    }

    THESIS_INLINE THESIS_HOST_DEVICE Optional& operator=(T&& v) noexcept {
        has_value_ = true;
        value_ = utility::move(v);
        return *this;
    }

    template <typename... Args>
    THESIS_INLINE THESIS_HOST_DEVICE void emplace(Args&&... args) {
        has_value_ = true;
        value_ = T(utility::forward<Args>(args)...);
    }

    THESIS_INLINE THESIS_HOST_DEVICE void reset() noexcept { has_value_ = false; }

    THESIS_INLINE THESIS_HOST_DEVICE bool has() const noexcept { return has_value_; }

    THESIS_INLINE THESIS_HOST_DEVICE operator bool() const noexcept { return has_value_; }

    THESIS_INLINE THESIS_HOST_DEVICE const T& operator*() const noexcept { return value_; }
    THESIS_INLINE THESIS_HOST_DEVICE T& operator*() noexcept { return value_; }

    THESIS_INLINE THESIS_HOST_DEVICE const T* operator->() const noexcept { return &value_; }
    THESIS_INLINE THESIS_HOST_DEVICE T* operator->() noexcept { return &value_; }
};

template <typename T, typename... Args>
THESIS_INLINE THESIS_HOST_DEVICE Optional<T> make_optional(Args&&... args) {
    return Optional<T>(utility::forward<Args>(args)...);
}

}  // namespace utils
}  // namespace device
}  // namespace thesis
