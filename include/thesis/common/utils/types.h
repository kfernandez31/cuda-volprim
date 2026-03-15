#pragma once

#include <cstdint>

using uint = unsigned int;

// Primitive index type: uint16_t suffices for ≤65535 primitives per scene
// Change to uint32_t if scenes exceed this
using prim_idx_t = uint16_t;
