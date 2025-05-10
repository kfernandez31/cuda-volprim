#pragma once

#include <span>
#include <cstddef>
#include <span>
#include <type_traits>

namespace thesis::data {

template <typename To, typename From>
inline std::span<const To> reinterpretSpan(std::span<const From> src) {
    static_assert(sizeof(From) == sizeof(To), "Size mismatch in reinterpretSpan");
    static_assert(std::is_trivially_copyable_v<From> && std::is_trivially_copyable_v<To>,
                  "Types must be trivially copyable for reinterpretSpan");
    return {reinterpret_cast<const To*>(src.data()), src.size()};
}

} // namespace thesis::data
