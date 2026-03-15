#pragma once

#include "thesis/common/utils/math.h"
#include "thesis/common/utils/types.h"

// Compile-time guards (define in CMakeLists.txt for debug/test builds):
// - THESIS_ENABLE_NUMERICAL_GUARDS: Enable runtime checks for numerical edge cases
//   (degenerate directions, invalid exit computations, etc.)
//   Adds ~5-10% overhead but catches numerical issues early
//   Recommended for: debug builds, testing, validation

namespace thesis {
namespace device {
namespace consts {

// Ray tracing constants
constexpr float INF_F = 1e20;
constexpr uint VISIBILITY_ALL = 0xFF;

// Primitive and scattering constants
// Maximum number of primitives that can be SIMULTANEOUSLY OVERLAPPING per ray
// This is NOT the total scene primitive count - it's the max depth of ray-primitive overlap
// Limited by OptiX continuation stack size (hardware/driver dependent)
// Stack allocation: Hit buffer (2×MAX × ~24 bytes) + Active set (~MAX × 8 bytes)
// Tested limits (OptiX compilation success):
//   - MAX_PRIMITIVES=128:  ~6KB stack (✓ safe on all GPUs)
//   - MAX_PRIMITIVES=256:  ~12KB stack (✓ safe on most GPUs)
//   - MAX_PRIMITIVES=512:  ~25KB stack (? test on your GPU)
//   - MAX_PRIMITIVES=1024: ~50KB stack (✓ RTX 3090, ? other GPUs)
//   - MAX_PRIMITIVES=2048: ~100KB stack (✗ OptiX compile error)
// If buffer overflow occurs: ray terminated early → BIASED RENDERING
constexpr size_t MAX_PRIMITIVES = 1024;

// Hit buffer capacity: only needs to hold entry hits (exits computed lazily in argmin)
// With argmin optimization, we never store exit hits in the buffer
constexpr size_t HIT_BUFFER_CAPACITY = MAX_PRIMITIVES;

// Active primitives set capacity: tracks unique primitive indices only
constexpr size_t ACTIVE_PRIMS_CAPACITY = MAX_PRIMITIVES;

// Minimum ray segment length for optical depth integration
// Segments shorter than this are considered degenerate
constexpr float RAY_SEGMENT_MIN_LENGTH = 1e-6f;

// Optical depth safety bounds (for single precision exp/log operations)
// Minimum optical depth to prevent log(0) errors
constexpr float MIN_OPTICAL_DEPTH = 1e-8f;

// Maximum optical depth before exp(-tau) underflows to zero
// exp(-88.0f) ≈ 1.4e-39 ≈ FLT_MIN in IEEE 754 single precision
constexpr float MAX_OPTICAL_DEPTH = 88.0f;

// =============================================================================
// Intersection Constants
// =============================================================================
// Ray-ellipsoid intersection numerical stability thresholds

// Minimum ray direction length squared to avoid division by near-zero
// Protects against degenerate rays in single precision
constexpr float RAY_DIRECTION_MIN_LENGTH2 = 1e-12f;

// Minimum discriminant value for valid intersection
// Protects discriminant evaluation from numerical errors in single precision
constexpr float INTERSECTION_DISCRIMINANT_EPS = 1e-12f;

// Minimum intersection segment length (t_exit - t_entry)
// Grazing angle hits below this threshold cause numerical instability in optical depth integration
constexpr float INTERSECTION_MIN_SEGMENT_LENGTH = 1e-8f;

// =============================================================================
// Path Tracing Constants
// =============================================================================
// Path tracing parameters (tune experimentally for your scenes)

// Maximum path depth before forced termination
constexpr size_t MAX_BOUNCES =
    128;  // Mitsuba production: 64-128, consider reducing to 64 after profiling

// Minimum throughput before path termination (prevents numerical underflow)
constexpr float MIN_THROUGHPUT = 1e-4f;

// Depth at which Russian roulette path termination begins
// Volumetric media with high albedo needs higher values than surface rendering
// Mitsuba surface rendering: 5-8, volumetric rendering may benefit from similar or higher
constexpr size_t RR_DEPTH = 5;  // Increased from 3 for proper volumetric path lengths

// Maximum survival probability for Russian roulette (maintains unbiasedness)
constexpr float RR_MAX_SURVIVAL = 0.99f;  // Standard production value (Mitsuba, PBRT)

// Isotropic phase function value: 1/(4π) normalized over unit sphere
constexpr float PHASE_VALUE = common::math::ONE_OVER_FOUR_PI_F;

// Next Event Estimation (NEE): compute shadow transmittance at scatter points
// When enabled, direct lighting is attenuated by transmittance along shadow ray
// When disabled, falls back to unoccluded single-scatter approximation
constexpr bool ENABLE_NEE = true;

// =============================================================================
// Adaptive Sampling Constants
// =============================================================================

// Minimum samples before convergence testing begins
// Central Limit Theorem requires sufficient samples for variance estimation
// Typical range: 10-30 samples (lower = more aggressive, higher = more conservative)
// Tune based on scene complexity: simple scenes can use 10, complex scenes need 20-30
constexpr size_t ADAPTIVE_MIN_SAMPLES = 32;

// Relative error threshold for convergence (default: 1%)
// Pixel converges when: max(std_dev / mean) across all channels < threshold
// Lower threshold = higher quality but less speedup
constexpr float ADAPTIVE_THRESHOLD = 0.01f;

// Minimum luminance to avoid division by zero in relative error computation
// Used when computing relative error for near-black pixels
constexpr float ADAPTIVE_MIN_LUMINANCE = 1e-6f;

}  // namespace consts
}  // namespace device
}  // namespace thesis
