#pragma once

#include "thesis/common/utils/math.h"
#include "thesis/common/utils/types.h"

#include <limits>

// Compile-time guards (define in CMakeLists.txt for debug/test builds):
// - THESIS_ENABLE_NUMERICAL_GUARDS: Enable runtime checks for numerical edge cases
//   (degenerate directions, invalid exit computations, etc.)
//   Adds ~5-10% overhead but catches numerical issues early
//   Recommended for: debug builds, testing, validation

namespace thesis {
namespace device {
namespace consts {

// Ray tracing constants
constexpr float INF_F = 1e20f;
constexpr uint VISIBILITY_ALL = 0xFF;

// Total number of primitives in the scene.
// Governs PrimsSet implementation selection:
//   ≤256: BitVector<N>    — O(1) insert/erase, N/8 bytes, indexed by prim ID
//   >256: CompactSet<N>   — O(k) insert/erase, 2*MAX_ACTIVE_PRIMS bytes, decoupled from scene size
constexpr size_t MAX_PRIMITIVES = 1024;  // >256 → uses CompactSet (supports any scene size)
static_assert(MAX_PRIMITIVES <= std::numeric_limits<prim_idx_t>::max(),
              "MAX_PRIMITIVES exceeds prim_idx_t capacity — widen prim_idx_t in "
              "include/thesis/common/utils/types.h");

// Max primitives simultaneously overlapping at a single point along a ray.
// Only used when MAX_PRIMITIVES > 256 (CompactSet mode).
// Cloud scene (652 Gaussians): measured max overlap = 37 at 1σ, 45 at 2σ.
// NOTE (2026-06-01): proven via collinear stress test that CompactSet SILENTLY
// DROPS overlapping prims past this cap → under-absorption (too bright). The cloud
// (652 Gaussians) was verified to stay <64 (old-caps vs 256-caps renders are
// BIT-IDENTICAL across all 24 cams), so this never affected the cloud. CompactSet
// is cheap (2 bytes/entry), so raised 64→128 for headroom on denser scenes at
// negligible cost. The expensive buffer is HIT_BUFFER_CAPACITY below; keep it
// modest. Proper fix for pathological overlap is graceful overflow, not a bigger
// cap — see SG_active_prims_cap_bug.png / SG_stress_trend.png.
constexpr size_t MAX_ACTIVE_PRIMS = 128;

// Hit buffer capacity: max entry hits stored per ray.
// On overflow the anyhit shader drops the excess hit but keeps traversing,
// so the env-map miss still resolves correctly.
// NOTE (2026-06-01): this is the EXPENSIVE per-ray buffer (HitRecord storage) —
// raising it to 256 made renders ~6× slower (local memory blowup). The cloud was
// verified to need ≤128 entries/ray (old-caps vs
// 256-caps cloud renders BIT-IDENTICAL), so kept at the validated 128. A ray
// crossing >128 prim entries (very dense / collinear-stack scenes) will drop hits
// → under-absorption; bump per-scene if needed, accepting the perf cost.
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
//
// RUNTIME-PROMOTED (Phase 1): MAX_BOUNCES, RR_DEPTH, RR_MAX_SURVIVAL, FIREFLY_CLAMP_LUMINANCE,
// PIXEL_FILTER_STDDEV and HG_G are now runtime params in common::params::RenderParams, set via
// CLI flags (--max-depth/--rr-depth/--rr-max-survival/--firefly-clamp/--filter-stddev/--hg-g).
// The constants below are the DEFAULTS (kept in sync with host Config defaults); they are no
// longer read on the device. HG_ISOTROPIC_EPS and PHASE_VALUE ARE still used (host-side HG fold).

// Maximum path depth before forced termination (default; runtime: --max-depth)
constexpr size_t MAX_BOUNCES =
    128;  // Mitsuba production: 64-128

// Minimum throughput before path termination (prevents numerical underflow)
constexpr float MIN_THROUGHPUT = 1e-4f;

// Optional firefly suppression: per-sample luminance clamp threshold (Rec.709 luma).
// 0 = DISABLED (default; bit-identical, unbiased — required for validation). When >0,
// any sample whose luminance exceeds this is scaled down to it (hue-preserving), killing
// low-probability high-weight spikes. BIASED (energy loss on clamped pixels) → beauty /
// robustness only, never for the Mitsuba comparison. Pick well above legit content
// luminance (sky ~1-2) so only true outliers clamp — e.g. 10-50 for the meadow.
constexpr float FIREFLY_CLAMP_LUMINANCE = 0.0f;

// Pixel reconstruction filter. 0 = BOX (default): uniform [-0.5,0.5] sub-pixel jitter =
// exact pixel-area average, the validation-exact reconstruction (matches Mitsuba run with
// rfilter=box). >0 = GAUSSIAN with this stddev (px), via filter importance sampling: the
// sub-pixel offset is drawn from the Gaussian kernel (support clamped to ±2px) and
// accumulated weight-1, so adjacent pixels gather overlapping regions → softer, less
// aliased edges (Mitsuba's hdrfilm default is gaussian stddev≈0.5). NO splatting/atomics —
// only the jitter distribution changes. Use for BEAUTY (esp. non-denoised shots); keep 0
// for the Mitsuba comparison. A flat field stays flat (energy preserved → furnace exact).
constexpr float PIXEL_FILTER_STDDEV = 0.0f;

// Depth at which Russian roulette path termination begins
// Volumetric media with high albedo needs higher values than surface rendering
// Mitsuba surface rendering: 5-8, volumetric rendering may benefit from similar or higher
constexpr size_t RR_DEPTH = 12;  // 3→5→12: high-albedo volumetrics need deep paths; depth-12 is the
                                 // measured efficiency optimum on the σ=7.5/albedo-0.9 cloud (~11% better
                                 // quality/sec than 5; FINDINGS §8.33). Unbiased; neutral on thin scenes.

// Maximum survival probability for Russian roulette (maintains unbiasedness)
constexpr float RR_MAX_SURVIVAL = 0.99f;  // Standard production value (Mitsuba, PBRT)

// Isotropic phase function value: 1/(4π). Used by the legacy unoccluded
// single-scatter accumulation; NEE/MIS paths use eval_phase() instead.
constexpr float PHASE_VALUE = common::math::ONE_OVER_FOUR_PI_F;

// Henyey-Greenstein anisotropy parameter g ∈ (-1, 1):
//   g = 0   → isotropic (HG reduces to 1/(4π))
//   g > 0   → forward-scattering (clouds typically use ~0.85)
//   g < 0   → back-scattering
// Default 0 keeps behavior identical to the prior isotropic implementation.
constexpr float HG_G = 0.85f;

// |g| below this threshold uses the isotropic branch (avoids 1/g in the inversion).
constexpr float HG_ISOTROPIC_EPS = 1e-3f;

// Next Event Estimation: at each scatter, sample a direction toward the env and
// attenuate by transmittance along a shadow ray. With NEE on, the unscattered-only
// escape rule kicks in (paths that scattered already counted env via shadow rays).
constexpr bool ENABLE_NEE = true;

// Multiple Importance Sampling between phase IS and env IS at scatter points.
// Mathematically a no-op when phase is isotropic AND env is uniform (both strategies
// degenerate to uniform sphere). Real benefit appears with HG_G != 0 OR a non-uniform
// HDR environment map. Costs 2× shadow rays per scatter, so disable on simple scenes.
// Requires ENABLE_NEE.
// VALIDATED 2026-06-03 (FINDINGS §8.10): furnace-energy exact, matches the phase-IS NEE
// estimator to 0.7σ, ~159× variance reduction on the meadow. (The prior energy bug was a
// phase::eval sign inconsistency, now fixed.) On for env-map scenes; no-op cost on constant env.
constexpr bool ENABLE_MIS = true;

// Volumetric product-RIS for the NEE direct term (OPTIMIZATION_FRONTIER ②). When true, replaces
// the 2-shadow-ray phase-IS+env-IS balance-MIS with a SINGLE shadow ray: K unshadowed env-IS
// candidates are resampled — weighted by the unshadowed product target p̂ = phase(wi,ω)·lum(env(ω))
// over the env-IS proposal pdf — into a 1-survivor weighted reservoir, and only the survivor's
// transmittance is traced. Unbiased RIS (Talbot et al. 2005, "Importance Resampling for Global
// Illumination"): recovers MIS-quality product sampling at ~1-ray cost (the 2nd shadow ray is
// ~26% of frame time, §8.36-adjacent kill-test). Requires ENABLE_NEE; takes precedence over
// ENABLE_MIS when both are set. K=1 reduces exactly to plain env-IS NEE.
constexpr bool ENABLE_RIS = true;
constexpr int RIS_NUM_CANDIDATES = 6;  // K (default; runtime --ris-candidates). K-sweep §8.37: the
                                       // equal-quality sweet spot is K=4–8 (~1.4–1.46×); 6 is the
                                       // chosen compromise (cost rises with K, quality plateaus by ~8).

// Analytic (Rao-Blackwellized) direct camera->env transmittance. When true, the
// bounce-0 unscattered term is added deterministically as throughput · exp(-τ) · env
// (via compute_transmittance_to_env) instead of the high-variance analog binary
// escape (env with probability exp(-τ)). Unbiased — replaces the direct-term
// estimator with its conditional expectation; scatter sampling/NEE untouched.
// Collapses MC noise on the unscattered component (the entire image when albedo=0).
constexpr bool ENABLE_ANALYTIC_DIRECT = true;

// =============================================================================
// Adaptive Sampling Constants
// =============================================================================

// Master gate. When false, variance buffer is not allocated and the kernel
// skips Welford's M2 update — saves W·H·16B of device memory + a write per
// sample. Flip to true when you actually want adaptive sampling.
constexpr bool ENABLE_ADAPTIVE_SAMPLING = false;

// Minimum samples before convergence testing begins
// Central Limit Theorem requires sufficient samples for variance estimation
// Typical range: 10-30 samples (lower = more aggressive, higher = more conservative)
// Tune based on scene complexity: simple scenes can use 10, complex scenes need 20-30
constexpr size_t ADAPTIVE_MIN_SAMPLES = 32;

// Relative error threshold for convergence (default: 1%)
// Pixel converges when: max(std_dev / mean) across all channels < threshold
// Lower threshold = higher quality but less speedup
constexpr float ADAPTIVE_THRESHOLD = 0.0f;  // Only consulted when ENABLE_ADAPTIVE_SAMPLING is true

// Minimum luminance to avoid division by zero in relative error computation
// Used when computing relative error for near-black pixels
constexpr float ADAPTIVE_MIN_LUMINANCE = 1e-6f;

}  // namespace consts
}  // namespace device
}  // namespace thesis
