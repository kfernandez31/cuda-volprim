#pragma once

#include "core/constants.cuh"

#include "thesis/common/utils/types.h"
#include "thesis/device/utils/bit_vector.h"
#include "thesis/device/utils/compact_set.h"

#include <type_traits>

namespace thesis {
namespace device {

// PrimsSet: tracks which primitives are active (overlapping) at the current ray point.
// For small scenes (≤256 prims): BitVector — O(1) ops, indexed by primitive ID.
// For large scenes: CompactSet — O(k) ops, decoupled from scene size.
//
// Extracted from device/core/sampling.cuh so it can also be embedded in the wavefront
// RayState (include/thesis/device/params/ray_state.h) without pulling in the full
// sampling/trace device stack. Host-safe: both BitVector and CompactSet expose their
// POD layout unconditionally and gate methods on __CUDA_ARCH__.
using PrimsSet =
    std::conditional_t<(consts::MAX_PRIMITIVES <= 256),
                       utils::BitVector<((consts::MAX_PRIMITIVES + 63) & ~size_t{63})>,
                       utils::CompactSet<prim_idx_t, consts::MAX_ACTIVE_PRIMS> >;

}  // namespace device
}  // namespace thesis
