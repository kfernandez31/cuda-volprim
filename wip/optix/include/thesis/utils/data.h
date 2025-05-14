#pragma once

#include <vector_types.h>

#include <cstddef>
#include <glm/glm.hpp>
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

inline float3 toFloat3(const glm::vec3& v) noexcept {
    return make_float3(v.x, v.y, v.z);
}

inline glm::vec3 toVec3(const float3& v) noexcept {
    return {v.x, v.y, v.z};
}

}  // namespace thesis::data
