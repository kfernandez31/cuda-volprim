#pragma once

#include <vector_types.h>

#include <cstddef>
#include <glm/glm.hpp>
#include <span>
#include <type_traits>

namespace thesis::host::utils::data {

// TODO(kacper): remove?
template <typename To, typename From>
[[nodiscard]] std::span<const To> reinterpretSpan(std::span<const From> src) {
    static_assert(sizeof(From) == sizeof(To), "Size mismatch in reinterpretSpan");
    static_assert(std::is_trivially_copyable_v<From> && std::is_trivially_copyable_v<To>,
                  "Types must be trivially copyable for reinterpretSpan");
    return {reinterpret_cast<const To*>(src.data()), src.size()};
}

[[nodiscard]] inline float3 toFloat3(glm::vec3 v) noexcept {
    return make_float3(v.x, v.y, v.z);
}

[[nodiscard]] inline float4 toFloat4(const glm::vec4& v) noexcept {
    return make_float4(v.x, v.y, v.z, v.w);
}

}  // namespace thesis::host::utils::data