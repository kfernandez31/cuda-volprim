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
// Maximum number of primitives that can be processed simultaneously per ray
constexpr size_t MAX_PRIMITIVES = 64;  // TODO(kacper): change based on realistic estimates

// Hit buffer capacity: must hold BOTH entry AND exit hits (2 hits per primitive)
// Each primitive generates one entry hit and one exit hit during ray traversal
constexpr size_t HIT_BUFFER_CAPACITY = 2 * MAX_PRIMITIVES;

// Active primitives set capacity: tracks unique primitive indices only
constexpr size_t ACTIVE_PRIMS_CAPACITY = MAX_PRIMITIVES;

// Epsilon values for numerical stability and geometric tolerances
// Used to detect coincident surface hits (primitives with surfaces at same t-value)
constexpr float HIT_COINCIDENCE_EPS = 1e-6f;

// Minimum ray segment length for optical depth integration
// Segments shorter than this are considered degenerate
constexpr float RAY_SEGMENT_MIN_LENGTH = 1e-6f;

// Minimum clamping value for random samples to prevent log(0) in importance sampling
// Used in tau = -log(1 - xi) where xi is uniform random sample
constexpr float MIN_RANDOM_SAMPLE = 1e-6f;

// Optical depth safety bounds (for single precision exp/log operations)
// Minimum optical depth to prevent log(0) errors
constexpr float MIN_OPTICAL_DEPTH = 1e-8f;

// Maximum optical depth before exp(-tau) underflows to zero
// exp(-88.0f) ≈ 1.4e-39 ≈ FLT_MIN in IEEE 754 single precision
constexpr float MAX_OPTICAL_DEPTH = 88.0f;

// Bisection search tolerance for distance sampling
// Balance between accuracy and iteration count (typically 4 iterations)
constexpr float BISECTION_DISTANCE_EPS = 1e-4f;

// Minimum optical depth difference in bisection to avoid infinite refinement
constexpr float BISECTION_TAU_EPS = 1e-6f;

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

}  // namespace consts
}  // namespace device
}  // namespace thesis
