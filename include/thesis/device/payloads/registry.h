#pragma once

#include "thesis/device/payloads/closesthit.h"
#include "thesis/device/payloads/miss.h"

#include <cstddef>

namespace thesis {
namespace device {
namespace payloads {

template <typename... Ts>
struct TypeList {};

using PayloadTypes = TypeList<ClosestHit, Miss>;

template <typename... Ts>
struct MaxCountImpl;

template <typename T>
struct MaxCountImpl<T> {
    static constexpr size_t value = T::Count;
};

template <typename T, typename... Ts>
struct MaxCountImpl<T, Ts...> {
    static constexpr size_t value =
        (T::Count > MaxCountImpl<Ts...>::value) ? T::Count : MaxCountImpl<Ts...>::value;
};

template <typename TypeList>
struct MaxCount;

template <typename... Ts>
struct MaxCount<TypeList<Ts...>> {
    static constexpr size_t value = MaxCountImpl<Ts...>::value;
};

constexpr size_t MAX_PAYLOADS_IN_USE =
    device::payloads::MaxCount<device::payloads::PayloadTypes>::value;
static_assert(MAX_PAYLOADS_IN_USE <= MAX_PAYLOADS, "Exceeds OptiX max payloads.");

}  // namespace payloads
}  // namespace device
}  // namespace thesis
