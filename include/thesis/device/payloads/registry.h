#pragma once

#include "thesis/common/utils/math.h"
#include "thesis/device/payloads/anyhit.h"
#include "thesis/device/payloads/miss.h"

#include <cstddef>

namespace thesis {
namespace device {
namespace payloads {

template <typename... Ts>
struct TypeList {};

using PayloadTypes = TypeList<Miss, AnyHit>;

template <typename... Ts>
struct MaxCount;

template <typename... Ts>
struct MaxCount<TypeList<Ts...>> {
    static constexpr size_t value = (common::math::max(Ts::Count...));
};

constexpr size_t MAX_PAYLOADS_IN_USE = MaxCount<PayloadTypes>::value;
static_assert(MAX_PAYLOADS_IN_USE <= MAX_PAYLOADS, "Exceeds OptiX max payloads.");

}  // namespace payloads
}  // namespace device
}  // namespace thesis
