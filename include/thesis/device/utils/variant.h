/* TODO(kacper): remove
#pragma once

#include "thesis/common/utils/preprocessor.h"

namespace thesis {
namespace device {
namespace utils {

template <typename A, typename B>
struct is_same {
    static constexpr bool value = false;
};

template <typename A>
struct is_same<A, A> {
    static constexpr bool value = true;
};

template <typename U, typename... Ts>
struct TagIndex;

template <typename U, typename First, typename... Rest>
struct TagIndex<U, First, Rest...> {
    static constexpr int value = is_same<U, First>::value ? 0 : 1 + TagIndex<U, Rest...>::value;
};

template <typename U>
struct TagIndex<U> {
    static_assert(sizeof(U) == 0, "Type not found in Variant");
    static constexpr int value = -1;
};

template <typename... Ts>
struct Variant;

template <typename T, typename... Ts>
struct Variant<T, Ts...> {
    static constexpr size_t MaxSize  = math::max(sizeof(T),  Variant<Ts...>::MaxSize);
    static constexpr size_t MaxAlign = math::max(alignof(T), Variant<Ts...>::MaxAlign);

    alignas(MaxAlign) char data[MaxSize];
    int tag = -1;  // no type yet

    template <typename U>
    __device__ void set(const U& value) noexcept {
        static_assert(TagIndex<U, T, Ts...>::value >= 0, "Type not in Variant");
        *reinterpret_cast<U*>(data) = value;
        tag = TagIndex<U, T, Ts...>::value;
    }

    template <typename U>
    __device__ U& get() noexcept {
        assert(tag == TagIndex<U, T, Ts...>::value && "Variant type mismatch");
        return *reinterpret_cast<U*>(data);
    }

    template <typename U>
    __device__ const U& get() const noexcept {
        assert(tag == TagIndex<U, T, Ts...>::value && "Variant type mismatch");
        return *reinterpret_cast<const U*>(data);
    }

    template <typename U>
    __device__ bool is() const noexcept {
        static_assert(TagIndex<U, T, Ts...>::value >= 0, "Type not in Variant");
        return tag == TagIndex<U, T, Ts...>::value;
    }
};

template <>
struct Variant<> {
    static constexpr size_t MaxSize  = 0;
    static constexpr size_t MaxAlign = 1;
};

}  // namespace utils
}  // namespace device
}  // namespace thesis

*/