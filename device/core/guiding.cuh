#pragma once
// =============================================================================
// Path guiding (OPTIMIZATION_FRONTIER ④) — DIRECTIONALITY DIAGNOSTIC (kill-test-for-the-kill-test)
// =============================================================================
// Before building the full offline oracle (grid + learn pass + guide-IS MIS strategy + two-pass
// mode), this measures the cheap NECESSARY condition: is the per-cell INCIDENT RADIANCE field
// directional enough for guiding to help at all? Guiding can only beat phase sampling where incident
// radiance is anisotropic. If the field is near-isotropic (the dense-cloud prediction — §8.27/§8.32
// "near-diffusive"), no guide — however perfect — helps → kill ④ cheaply, skip the full oracle.
//
// Mechanism: deposit the NEE-found incident radiance L_i(ω)=env(ω)·T(ω) into a GUIDE_SPATIAL_RES³
// grid of GUIDE_DIR_RES² octahedral directional bins (atomicAdd — harvested from NEE we already do).
// Host downloads the grid; Python computes per-cell directionality d=|Σ_bin v·ω̂_bin|/Σ_bin v
// (0=isotropic → guiding useless; →1 = single direction → guiding can help), aggregated over cells
// weighted by radiance. ONE shared read/write table (§8.34-safe: not per-ray state).
//
// NOT NOVEL (Vorba 2014 / Müller 2017 "Practical Path Guiding") — an adaptation. Directional
// histogram, not raw SH (SH goes negative / isn't invertible). The full oracle (sample()/pdf() as a
// 3rd MIS strategy, equal-area octahedral for an exact pdf) is built ONLY if this diagnostic survives.
// =============================================================================

#include "core/constants.cuh"

#include "thesis/common/utils/math.h"

#include <vector_types.h>

namespace thesis {
namespace device {
namespace guide {

namespace math = ::thesis::common::math;

// world position → flat spatial cell index, given the grid AABB (min + per-axis 1/extent).
__device__ __forceinline__ int cell_index(float3 aabb_min, float3 aabb_inv_extent, float3 p) {
    const float ux = (p.x - aabb_min.x) * aabb_inv_extent.x;  // → [0,1)
    const float uy = (p.y - aabb_min.y) * aabb_inv_extent.y;
    const float uz = (p.z - aabb_min.z) * aabb_inv_extent.z;
    const int x = math::clamp((int)(ux * consts::GUIDE_SPATIAL_RES), 0, consts::GUIDE_SPATIAL_RES - 1);
    const int y = math::clamp((int)(uy * consts::GUIDE_SPATIAL_RES), 0, consts::GUIDE_SPATIAL_RES - 1);
    const int z = math::clamp((int)(uz * consts::GUIDE_SPATIAL_RES), 0, consts::GUIDE_SPATIAL_RES - 1);
    return (z * consts::GUIDE_SPATIAL_RES + y) * consts::GUIDE_SPATIAL_RES + x;
}

// unit direction → octahedral [0,1]² → DIR_RES×DIR_RES bin (Cigolle et al. 2014). For the DIAGNOSTIC
// the plain (≈equal-area) map is fine — exactness only matters for the oracle's pdf. The host readout
// uses the matching inverse (oct_decode) in Python.
__device__ __forceinline__ int dir_to_bin(float3 d) {
    const float inv_l1 = 1.0f / (fabsf(d.x) + fabsf(d.y) + fabsf(d.z));
    float px = d.x * inv_l1, py = d.y * inv_l1;
    if (d.z < 0.0f) {
        const float ox = (1.0f - fabsf(py)) * math::copysign(1.0f, px);
        const float oy = (1.0f - fabsf(px)) * math::copysign(1.0f, py);
        px = ox;
        py = oy;
    }
    const int bu =
        math::clamp((int)((px * 0.5f + 0.5f) * consts::GUIDE_DIR_RES), 0, consts::GUIDE_DIR_RES - 1);
    const int bv =
        math::clamp((int)((py * 0.5f + 0.5f) * consts::GUIDE_DIR_RES), 0, consts::GUIDE_DIR_RES - 1);
    return bv * consts::GUIDE_DIR_RES + bu;
}

// Deposit incident radiance arriving from `from_dir` into cell `c` (diagnostic write path).
__device__ __forceinline__ void deposit(float* bins, int c, float3 from_dir, float radiance) {
    if (bins != nullptr && radiance > 0.0f && isfinite(radiance)) {
        atomicAdd(&bins[c * consts::GUIDE_DIR_BINS + dir_to_bin(from_dir)], radiance);
    }
}

}  // namespace guide
}  // namespace device
}  // namespace thesis
