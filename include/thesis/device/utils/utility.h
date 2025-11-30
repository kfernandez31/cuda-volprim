#pragma once

#include <type_traits>

namespace thesis {
namespace device {
namespace utility {

template <typename T>
__device__ __forceinline__ constexpr T&& forward(typename std::remove_reference<T>::type& t) {
    return static_cast<T&&>(t);
}

template <typename T>
__device__ __forceinline__ constexpr T&& forward(typename std::remove_reference<T>::type&& t) {
    static_assert(!std::is_lvalue_reference<T>::value, "bad forward");
    return static_cast<T&&>(t);
}

template <typename T>
__device__ __forceinline__ void swap(T& a, T& b) {
    T tmp = a;
    a = b;
    b = tmp;
}

}  // namespace utility
}  // namespace device
}  // namespace thesis
