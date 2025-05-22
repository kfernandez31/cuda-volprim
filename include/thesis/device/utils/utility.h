#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <type_traits>

namespace thesis {
namespace device {
namespace utility {

template <typename T>
THESIS_INLINE THESIS_HOST_DEVICE constexpr T&& forward(typename std::remove_reference<T>::type& t) noexcept {
    return static_cast<T&&>(t);
}

template <typename T>
THESIS_INLINE THESIS_HOST_DEVICE constexpr T&& forward(typename std::remove_reference<T>::type&& t) noexcept {
    static_assert(!std::is_lvalue_reference<T>::value, "bad forward");
    return static_cast<T&&>(t);
}

template <typename T>
THESIS_INLINE THESIS_HOST_DEVICE constexpr typename std::remove_reference<T>::type&& move(T&& t) noexcept {
    return static_cast<typename std::remove_reference<T>::type&&>(t);
}

template <typename T>
THESIS_INLINE THESIS_HOST_DEVICE void swap(T& a, T& b) noexcept {
    T tmp = move(a);
    a = move(b);
    b = move(tmp);
}

template <typename T, typename U = T>
THESIS_INLINE THESIS_HOST_DEVICE T exchange(T& obj, U&& new_value) noexcept {
    T old = move(obj);
    obj = forward<U>(new_value);
    return old;
}

template <typename T, typename U = T>
THESIS_INLINE THESIS_HOST_DEVICE T exchange(T& obj, U&& new_value) noexcept {
    T old = move(obj);
    obj = forward<U>(new_value);
    return old;
}

}  // namespace utility
}  // namespace device
}  // namespace thesis
