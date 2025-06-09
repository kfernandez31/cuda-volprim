#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/utils/binary_set.h"
#include "thesis/device/utils/linear_set.h"
#include "thesis/device/utils/utility.h"

#include <cstddef>
#include <type_traits>

namespace thesis {
namespace device {
namespace utils {

constexpr size_t SET_THRESHOLD = 32;

// TODO(kacper): join them with a SetBase under one header
template <typename T, size_t Capacity>
// clang-format off
using Set = typename std::conditional_t<(Capacity > SET_THRESHOLD), 
    BinarySet<T, Capacity>, 
    LinearSet<T, Capacity>
>;

}  // namespace utils
}  // namespace device
}  // namespace thesis
