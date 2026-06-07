#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/common/utils/types.h"
#include "thesis/device/params/camera.h"
#include "thesis/device/params/environment_map.h"
#include "thesis/device/params/image.h"
#include "thesis/device/params/primitive.h"
#include "thesis/device/utils/vector.h"

#include <optix.h>
#include <vector_types.h>

#include <cstddef>

namespace thesis {
namespace common {
namespace params {

// Runtime-configurable render parameters. These were compile-time constants in
// device/core/constants.cuh; promoting them to launch params lets a single binary render
// validation / beauty / parity configs without a rebuild (and kills the CUDA-vs-Mitsuba
// rebuild asymmetry for --hg-g / --max-depth). Defaults MIRROR constants.cuh.
//
// UNBIASED, NOT bit-exact vs the old constexpr build: with the defaults the math is identical,
// but under -use_fast_math the optimizer's FMA-contraction/codegen differs by ~1 ULP when a
// value is a runtime load rather than a compile-time constant. That 1-ULP shift flips the
// single most-marginal MC decision in an image (e.g. a Russian-roulette survive/die at a
// boundary), so ~0.03% of pixels differ with ZERO mean (furnace 1.00006, signed-mean ~3e-10,
// energy ratio 1.0000000). Same accepted class as the incremental-active-prims opt. The
// against-Mitsuba systematic gate is unaffected (mean/energy identical).
//
// The Henyey-Greenstein constants are PRE-FOLDED on the host (hg_g2_, one±g2, neg_inv_2g) so
// the per-sample device arithmetic is the same FMA sequence the constexpr version used. eval
// uses +g (hg_g_), sample uses g_eff = -g (hg_g_eff_); see device/core/sampling.cuh phase
// namespace for the sign convention.
struct RenderParams {
    size_t max_bounces_ = 128;            // was consts::MAX_BOUNCES
    size_t rr_depth_ = 5;                 // was consts::RR_DEPTH
    float rr_max_survival_ = 0.99f;       // was consts::RR_MAX_SURVIVAL
    float firefly_clamp_luminance_ = 0.0f;  // was consts::FIREFLY_CLAMP_LUMINANCE (0 = off)
    float pixel_filter_stddev_ = 0.0f;    // was consts::PIXEL_FILTER_STDDEV (0 = box)

    bool hg_isotropic_ = false;           // |g| < eps → isotropic branch (host decides)
    float hg_g_ = 0.85f;                  // user-facing g (eval uses +g); was consts::HG_G
    float hg_g_eff_ = -0.85f;             // -g, used by sample(); was sampling.cuh HG_G_EFF
    float hg_g2_ = 0.7225f;               // g²
    float hg_one_plus_g2_ = 1.7225f;      // 1 + g²
    float hg_one_minus_g2_ = 0.2775f;     // 1 - g²
    float hg_neg_inv_2g_ = -0.5f / -0.85f;  // -1/(2·g_eff)
    // PHASE_VALUE·(1-g²) pre-folded into ONE constant, matching the constexpr eval_hg which
    // folded these two constants at compile time. Keeps eval to a single runtime multiply
    // (× inv_denom_3_2) — a micro-opt; not sufficient for full bit-exactness (see struct note).
    float hg_phase_coeff_ = 0.0f;

    // Adaptive sampling (runtime-gated; was the compile-time ENABLE_ADAPTIVE_SAMPLING +
    // ADAPTIVE_THRESHOLD). The host allocates the per-pixel variance (Welford M2) buffer iff
    // adaptive_threshold_ > 0; the device then keys all adaptive work on image_.variance_ != null.
    // adaptive_threshold_ = target RELATIVE STANDARD ERROR OF THE MEAN: a pixel stops sampling
    // once max_channel( stderr/mean ) < threshold, where stderr = sqrt(M2/((n-1)·n)). So 0.02 =
    // "stop at 2% estimated relative error." 0 = OFF (no buffer, no per-sample M2 cost,
    // bit-identical to a non-adaptive build). UNBIASED: the per-pixel estimate is the mean of
    // however many samples it took — stopping early only leaves a pixel slightly noisier, never
    // biased (the Welford mean is output directly). Set the threshold conservatively so stopped
    // pixels are already converged (quality preserved); see FINDINGS §8.30.
    float adaptive_threshold_ = 0.0f;        // was consts::ADAPTIVE_THRESHOLD
    uint32_t adaptive_min_samples_ = 32;     // was consts::ADAPTIVE_MIN_SAMPLES (min before testing)
};

struct THESIS_ALIGNMENT LaunchParams {
    OptixTraversableHandle ias_handle_;
    device::params::Camera camera_;
    device::params::EnvironmentMap env_map_;
    device::params::Image image_;
    device::utils::DynamicVector<device::params::Primitive> primitives_;
    uint seed_;

    // Runtime render knobs (depth, RR, firefly clamp, pixel filter, HG anisotropy).
    RenderParams render_;

    // Device-side atomic counter for cap-overflow events (CompactSet active-prims or
    // the primary-ray HitBuffer dropping an entry). Bumped from device code, read back
    // by the host after the render to WARN that dense-overlap regions may be biased
    // (under-absorption) — turning the previously SILENT overflow into a visible signal.
    // Single element; null-guarded on device.
    unsigned long long* overflow_counter_ = nullptr;
};

}  // namespace params
}  // namespace common
}  // namespace thesis
