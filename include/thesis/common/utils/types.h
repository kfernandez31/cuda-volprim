#pragma once

#include <cstdint>

// Global alias kept intentionally: CUDA/OptiX code uses bare `uint` pervasively
// (e.g., __ldg<uint>, OptixInstance::instanceId, payload registers), and 40+
// call sites would need rewriting for theoretical BSD collision protection
// that's irrelevant on this Linux-only project.
using uint = unsigned int;

namespace thesis::common::utils {

// Primitive index type: uint16_t supports up to 65,535 primitives per scene.
// Cap is intentional — keeps hit-buffer entries small (relevant for the
// per-ray BitVector/CompactSet scratch in device code). Larger scenes need
// either a wider type here or a different traversal strategy.
using prim_idx_t = std::uint16_t;

}  // namespace thesis::common::utils

// Re-export at top of namespace tree so existing call sites can keep `prim_idx_t`
// unqualified. Project-specific name → no global-pollution risk.
using thesis::common::utils::prim_idx_t;
