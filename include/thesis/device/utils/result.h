#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/utils/utility.h"

namespace thesis {
namespace device {
namespace utils {

template <typename T, typename E>
struct Result {
    bool is_ok_;
    union {
        T ok_value_;
        E err_value_;
    };

    THESIS_HOST_DEVICE Result() : is_ok_(false), err_value_() {}

    THESIS_HOST_DEVICE Result(const T& ok) : is_ok_(true), ok_value_(ok) {}
    THESIS_HOST_DEVICE Result(T&& ok) : is_ok_(true), ok_value_(utility::move(ok)) {}

    THESIS_HOST_DEVICE Result(const E& err) : is_ok_(false), err_value_(err) {}
    THESIS_HOST_DEVICE Result(E&& err) : is_ok_(false), err_value_(utility::move(err)) {}

    THESIS_HOST_DEVICE Result(Result&& other) noexcept : is_ok_(other.is_ok_) {
        if (is_ok_)
            ok_value_ = utility::exchange(other.ok_value_, T{});
        else
            err_value_ = utility::exchange(other.err_value_, E{});
    }

    THESIS_HOST_DEVICE Result& operator=(Result&& other) noexcept {
        is_ok_ = other.is_ok_;
        if (is_ok_)
            ok_value_ = utility::exchange(other.ok_value_, T{});
        else
            err_value_ = utility::exchange(other.err_value_, E{});
        return *this;
    }

    Result(const Result&) = default;
    Result& operator=(const Result&) = default;

    template <typename... Args>
    THESIS_INLINE THESIS_HOST_DEVICE void emplace_ok(Args&&... args) noexcept {
        is_ok_ = true;
        ok_value_ = T(utility::forward<Args>(args)...);
    }

    template <typename... Args>
    THESIS_INLINE THESIS_HOST_DEVICE void emplace_err(Args&&... args) noexcept {
        is_ok_ = false;
        err_value_ = E(utility::forward<Args>(args)...);
    }

    THESIS_INLINE THESIS_HOST_DEVICE bool is_ok() const noexcept { return is_ok_; }
    THESIS_INLINE THESIS_HOST_DEVICE bool is_err() const noexcept { return !is_ok_; }

    THESIS_INLINE THESIS_HOST_DEVICE const T& unwrap() const noexcept { return ok_value_; }
    THESIS_INLINE THESIS_HOST_DEVICE T& unwrap() noexcept { return ok_value_; }

    THESIS_INLINE THESIS_HOST_DEVICE const E& unwrap_err() const noexcept { return err_value_; }
    THESIS_INLINE THESIS_HOST_DEVICE E& unwrap_err() noexcept { return err_value_; }

    THESIS_INLINE THESIS_HOST_DEVICE operator bool() const noexcept { return is_ok_; }

    THESIS_INLINE THESIS_HOST_DEVICE const T& operator*() const noexcept { return ok_value_; }
    THESIS_INLINE THESIS_HOST_DEVICE T& operator*() noexcept { return ok_value_; }

    THESIS_INLINE THESIS_HOST_DEVICE const T* operator->() const noexcept { return &ok_value_; }
    THESIS_INLINE THESIS_HOST_DEVICE T* operator->() noexcept { return &ok_value_; }
};

template <typename T, typename E, typename... Args>
THESIS_INLINE THESIS_HOST_DEVICE Result<T, E> make_ok(Args&&... args) {
    return Result<T, E>(T(utility::forward<Args>(args)...));
}

template <typename T, typename E, typename... Args>
THESIS_INLINE THESIS_HOST_DEVICE Result<T, E> make_err(Args&&... args) {
    return Result<T, E>(E(utility::forward<Args>(args)...));
}

}  // namespace utils
}  // namespace device
}  // namespace thesis
