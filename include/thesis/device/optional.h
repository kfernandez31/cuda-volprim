#pragma once

#include "thesis/utils/preprocessor.h"
#include "thesis/utils/utility.h"

namespace thesis {
namespace device {

struct NullOptTag {
    explicit constexpr NullOptTag() = default;
};

constexpr NullOptTag nullopt{};

template <typename T>
struct Optional {
    bool has_value;
    T value; // TODO(kacper): underscore_

    Optional() : has_value(false) {}
    Optional(NullOptTag) : has_value(false) {}

    template <typename U>
    Optional(U&& v)
        : has_value(true), value(utility::forward<U>(v)) {}

    Optional(const Optional&) = default;
    Optional(Optional&&) = default;

    THESIS_INLINE THESIS_HOST_DEVICE Optional& operator=(const T& v) = default;{
        has_value = true;
        value = v;
        return *this;
    }

    THESIS_INLINE THESIS_HOST_DEVICE Optional& operator=(T&& v) noexcept {
        has_value = true;
        value = utility::move(v);
        return *this;
    }

    THESIS_INLINE THESIS_HOST_DEVICE Optional& operator=(const Optional&) = default;

    THESIS_INLINE THESIS_HOST_DEVICE Optional& operator=(Optional&& other) noexcept {
        has_value = utility::exchange(other.has_value, false);
        value = utility::exchange(other.value, 0);
        return *this;
    }

    THESIS_INLINE THESIS_HOST_DEVICE void reset() noexcept {
        has_value = false;
    }

    THESIS_INLINE THESIS_HOST_DEVICE bool has() const noexcept {
        return has_value;
    }

    THESIS_INLINE THESIS_HOST_DEVICE operator bool() const noexcept {
        return has_value;
    }

    THESIS_INLINE THESIS_HOST_DEVICE const T& operator*() const noexcept {
        return value;
    }

    THESIS_INLINE THESIS_HOST_DEVICE T& operator*() noexcept {
        return value;
    }

    THESIS_INLINE THESIS_HOST_DEVICE const T* operator->() const noexcept {
        return &value;
    }

    THESIS_INLINE THESIS_HOST_DEVICE T* operator->() noexcept {
        return &value;
    }

    template <typename... Args>
    THESIS_INLINE THESIS_HOST_DEVICE void emplace(Args&&... args) {
        has_value = true;
        value = T(utility::forward<Args>(args)...);
    }
};

}  // namespace device
}  // namespace thesis