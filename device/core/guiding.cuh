#pragma once
// =============================================================================
// SCAFFOLD — path guiding via a shared spatio-directional grid (OPTIMIZATION_FRONTIER ④)
// =============================================================================
// STATUS: groundwork / SKETCH. NOT built, NOT wired in, NOT validated. Behind
// guide::ENABLE_GUIDE (default false). This file compiles to nothing until included +
// the flag is on. The point is to make the design concrete and show the moving parts.
//
// WHAT IT ATTACKS: the *surviving* cloud noise is multiple scattering — deep continuation
// bounces. Today the continuation direction is an unguided `phase::sample` (knows the HG
// lobe, blind to where light actually is in this region). Guiding learns, per region of
// space, a coarse "where does radiance come from here?" distribution and samples the
// continuation from it, MIS-combined with phase sampling. (RIS / §8.37 fixed the DIRECT
// term; this targets the INDIRECT term — the complementary half of the noise.)
//
// WHY IT'S ALLOWED (vs the dead wavefront §8.34): this is ONE shared, read-mostly table
// (~NUM_CELLS·DIR_BINS·4B ≈ 32³·64·4 ≈ 8 MB), high cache reuse, NOT per-ray state streamed
// through global memory. It does not trip the L2 cliff.
//
// DISCIPLINE (do NOT skip): build the OFFLINE ORACLE first (bake the grid from a high-spp
// pre-render, freeze it, render read-only) and measure RMSE²·time vs MIS. If even the
// perfect oracle doesn't beat MIS by >~1.15×, KILL before writing the online machinery.
//
// NOT NOVEL: path guiding is established — Vorba et al. 2014 (online GMM guiding),
// Müller et al. 2017 "Practical Path Guiding" (adaptive spatial + directional quadtree).
// This is an *adaptation* to the Gaussian medium, not an invention. (Müller uses a
// directional quadtree, not a fixed grid — a fixed octahedral grid is the simplest
// samplable stand-in for a first cut; swap in a quadtree later if the oracle pays off.)
//
// CORRECTNESS LANDMINE (same class as the env-IS texel-center bug, §8.37): the directional
// representation must be SAMPLABLE and its pdf must match the sampling density EXACTLY, or
// MIS is biased. Raw spherical harmonics (the frontier doc's first suggestion) are a poor
// fit — they go negative (invalid pdf) and aren't directly invertible. So this scaffold
// uses a per-cell DIRECTIONAL HISTOGRAM over an EQUAL-AREA octahedral map: directly
// samplable (pick a bin ∝ weight, jitter within it) and accumulable (atomicAdd into a bin).
// EQUAL-AREA is load-bearing: it makes every bin exactly 4π/DIR_BINS sr, so the solid-angle
// pdf is trivially P(bin)·DIR_BINS/(4π). A plain (non-equal-area) octahedral map would need
// the per-texel Jacobian or it biases — see oct_* TODO below.
// =============================================================================

#include "core/constants.cuh"
#include "core/random.cuh"

#include "thesis/common/utils/math.h"

#include <vector_types.h>

namespace thesis {
namespace device {
namespace guide {

namespace math = ::thesis::common::math;

// ─── compile-time gate + dimensions (tune in the oracle phase) ───
constexpr bool ENABLE_GUIDE = false;   // master switch (default OFF — scaffold)
constexpr int  GUIDE_MIN_DEPTH = 2;    // only guide deep continuation vertices (direct = RIS/MIS)
constexpr int  SPATIAL_RES = 32;       // 32³ spatial cells over the scene AABB
constexpr int  DIR_RES     = 8;        // DIR_RES×DIR_RES octahedral bins per cell
constexpr int  DIR_BINS    = DIR_RES * DIR_RES;                       // 64
constexpr int  NUM_CELLS   = SPATIAL_RES * SPATIAL_RES * SPATIAL_RES; // 32768
constexpr float INV_DIR_BINS_SR = (float)DIR_BINS / (4.0f * math::PI_F);  // P(bin) → sr⁻¹ (equal-area)

struct Sample { float3 wo; float pdf; };  // mirrors phase::Sample / env_is::Sample

// Device-visible grid view. Pointers live in launch_params (allocated host-side). Read-mostly
// on the hot path; `bins_` is the only thing the online write path touches (atomicAdd).
struct GuideGrid {
    float3 aabb_min_;          // scene AABB min (world)
    float3 aabb_inv_extent_;   // 1 / (aabb_max - aabb_min), per axis (degenerate-axis floored)
    float* bins_;              // [NUM_CELLS * DIR_BINS] accumulated incident radiance (Σ luminance)
    float* cell_cdf_;          // [NUM_CELLS * DIR_BINS] per-cell prefix sums of bins_, end-normalized
    float* cell_sum_;          // [NUM_CELLS] Σ bins per cell (0 ⇒ cell unlearned → caller falls back)
};

// ─── spatial: world position → flat cell index ───
__device__ __forceinline__ int cell_index(const GuideGrid& g, float3 p) {
    const float ux = (p.x - g.aabb_min_.x) * g.aabb_inv_extent_.x;  // → [0,1)
    const float uy = (p.y - g.aabb_min_.y) * g.aabb_inv_extent_.y;
    const float uz = (p.z - g.aabb_min_.z) * g.aabb_inv_extent_.z;
    const int x = math::clamp((int)(ux * SPATIAL_RES), 0, SPATIAL_RES - 1);
    const int y = math::clamp((int)(uy * SPATIAL_RES), 0, SPATIAL_RES - 1);
    const int z = math::clamp((int)(uz * SPATIAL_RES), 0, SPATIAL_RES - 1);
    return (z * SPATIAL_RES + y) * SPATIAL_RES + x;
}

// ─── directional: octahedral map  dir↔[0,1]²  (Cigolle et al. 2014) ───
// TODO(correctness): swap for the EQUAL-AREA octahedral map (Clarberg 2008) so every bin is
// exactly 4π/DIR_BINS sr and the pdf below is exact. This plain map is ~equal-area only; using
// it as-is biases MIS at the few-e-4 level (cf. the env-IS texel-center bug, §8.37). Sketch only.
__device__ __forceinline__ float2 oct_encode(float3 d) {
    const float inv_l1 = 1.0f / (fabsf(d.x) + fabsf(d.y) + fabsf(d.z));
    float px = d.x * inv_l1, py = d.y * inv_l1;
    if (d.z < 0.0f) {
        const float ox = (1.0f - fabsf(py)) * math::copysign(1.0f, px);
        const float oy = (1.0f - fabsf(px)) * math::copysign(1.0f, py);
        px = ox; py = oy;
    }
    return make_float2(px * 0.5f + 0.5f, py * 0.5f + 0.5f);   // [0,1]²
}
__device__ __forceinline__ float3 oct_decode(float2 e) {
    const float fx = e.x * 2.0f - 1.0f, fy = e.y * 2.0f - 1.0f;
    float3 n = make_float3(fx, fy, 1.0f - fabsf(fx) - fabsf(fy));
    const float t = math::max(-n.z, 0.0f);
    n.x += (n.x >= 0.0f) ? -t : t;
    n.y += (n.y >= 0.0f) ? -t : t;
    return math::normalize(n);
}
__device__ __forceinline__ int dir_to_bin(float3 d) {
    const float2 e = oct_encode(d);
    const int bu = math::clamp((int)(e.x * DIR_RES), 0, DIR_RES - 1);
    const int bv = math::clamp((int)(e.y * DIR_RES), 0, DIR_RES - 1);
    return bv * DIR_RES + bu;
}

// ─── READ PATH (used by both the oracle and the online version) ───
// Sample a continuation direction from cell `c`'s learned histogram + its solid-angle pdf.
// Returns pdf<=0 if the cell is unlearned (caller must fall back to phase sampling).
__device__ __forceinline__ Sample sample(const GuideGrid& g, int c, random::PCG32& rng) {
    if (g.cell_sum_[c] <= 0.0f) return {make_float3(0, 0, 1), 0.0f};   // unlearned → fall back
    const float* cdf = g.cell_cdf_ + c * DIR_BINS;                      // end-normalized to 1
    const float u = random::sample_uniform(rng);
    // pick a bin ∝ histogram (small linear/binary search over DIR_BINS — 64 entries)
    int bin = 0;
    while (bin < DIR_BINS - 1 && cdf[bin] < u) ++bin;
    // continuous direction: jitter uniformly inside the bin (NOT the bin center — same lesson
    // as env-IS §8.37), then octahedral-decode.
    const float2 jit = random::sample_uniform_2d(rng);
    const int bu = bin % DIR_RES, bv = bin / DIR_RES;
    const float2 e = make_float2((bu + jit.x) / (float)DIR_RES, (bv + jit.y) / (float)DIR_RES);
    const float3 wo = oct_decode(e);
    const float p_bin = g.bins_[c * DIR_BINS + bin] / g.cell_sum_[c];   // discrete bin probability
    return {wo, p_bin * INV_DIR_BINS_SR};                              // → solid-angle pdf (equal-area)
}

// Evaluate the guide pdf for a given direction (needed for MIS). Must match sample()'s density.
__device__ __forceinline__ float pdf(const GuideGrid& g, int c, float3 d) {
    if (g.cell_sum_[c] <= 0.0f) return 0.0f;
    const float p_bin = g.bins_[c * DIR_BINS + dir_to_bin(d)] / g.cell_sum_[c];
    return p_bin * INV_DIR_BINS_SR;
}

// ─── WRITE PATH (online learning ONLY — the oracle bakes bins_ offline and skips this) ───
// Deposit observed incident radiance arriving from `from_dir` at a point in cell `c`.
// Call it with the NEE-found env luminance (T·env) we already compute — near-free to harvest.
__device__ __forceinline__ void deposit(const GuideGrid& g, int c, float3 from_dir, float radiance) {
    if (radiance > 0.0f) atomicAdd(&g.bins_[c * DIR_BINS + dir_to_bin(from_dir)], radiance);
}
// NB: cell_cdf_ + cell_sum_ are rebuilt from bins_ between passes by a tiny host-launched
// kernel (one thread per cell: prefix-sum its DIR_BINS, store the total). Not shown here.

}  // namespace guide
}  // namespace device
}  // namespace thesis

// =============================================================================
// INTEGRATION SKETCH (not applied — shows where this plugs into device/entry/raygen.cuh)
// =============================================================================
//
// (A) CONTINUATION sampling — replace `event.direction_ = phase::sample(wi, rng).wo;`
//     with one-sample MIS between phase-IS and guide-IS at deep vertices:
//
//   if constexpr (guide::ENABLE_GUIDE) {
//     if (bounce >= guide::GUIDE_MIN_DEPTH) {
//       const int c = guide::cell_index(launch_params.guide_, event.position_);
//       const auto gs = guide::sample(launch_params.guide_, c, rng);
//       float3 wo;
//       if (gs.pdf > 0.0f && random::sample_uniform(rng) < 0.5f) wo = gs.wo;     // 50/50 strategy pick
//       else                                                     wo = phase::sample(wi, rng).wo;
//       const float p_phase = phase::pdf(wi, wo);
//       const float p_guide = guide::pdf(launch_params.guide_, c, wo);           // 0 if unlearned
//       const float mis_pdf = (gs.pdf > 0.0f) ? 0.5f * (p_phase + p_guide) : p_phase;
//       throughput *= phase::eval(wi, wo) * math::rcp(mis_pdf);  // f/pdf; ==1 when guide absent
//       event.direction_ = wo;
//     } else { event.direction_ = phase::sample(wi, rng).wo; }   // direct-ish bounces: RIS/MIS handle env
//   }
//   // (with ENABLE_GUIDE=false this whole block compiles out → today's behavior, bit-identical)
//
// (B) LEARNING (online only) — harvest the NEE result we already compute. In the NEE block,
//     after the env contribution `L_nee` toward direction `d_light` is known:
//
//   if constexpr (guide::ENABLE_GUIDE) {
//     const float lum = math::dot(L_nee, make_float3(0.2126f, 0.7152f, 0.0722f));
//     guide::deposit(launch_params.guide_, guide::cell_index(launch_params.guide_, event.position_),
//                    d_light, lum);
//   }
//
// (C) HOST (renderer.cpp): allocate the 3 buffers (bins_/cell_cdf_/cell_sum_), set aabb_min_/
//     inv_extent_ from utils::math::computeBounds(primitives) (already computed for Morton sort),
//     stash pointers in launch_params_.guide_. ORACLE path: run a learning-only pre-pass
//     (ENABLE_GUIDE off for sampling, deposit on) at high spp → rebuild cdf → freeze → render
//     with guide-sampling on. Compare RMSE²·time vs the MIS baseline. >1.15× or kill.
// =============================================================================
