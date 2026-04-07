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

// Total number of primitives in the scene.
// Governs PrimsSet implementation selection:
//   ≤256: BitVector<N>    — O(1) insert/erase, N/8 bytes, indexed by prim ID
//   >256: CompactSet<N>   — O(k) insert/erase, 2*MAX_ACTIVE_PRIMS bytes, decoupled from scene size
constexpr size_t MAX_PRIMITIVES = 1024;  // >256 → uses CompactSet (supports any scene size)

// Max primitives simultaneously overlapping at a single point along a ray.
// Only used when MAX_PRIMITIVES > 256 (CompactSet mode).
// Cloud scene (652 Gaussians): measured max overlap = 37 at 1σ, 45 at 2σ.
constexpr size_t MAX_ACTIVE_PRIMS = 64;

// Hit buffer capacity: max entry hits stored per ray.
// Overflow handled by optixTerminateRay() in anyhit → biased but safe.
constexpr size_t HIT_BUFFER_CAPACITY = 128;

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
