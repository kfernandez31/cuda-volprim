#pragma once

#include "scenes/cloud_validation.h"  // for MultiViewTestScene + CameraView

#include "thesis/host/utils/result.h"

namespace thesis::test::scenes {

// Verification scene for the systematic-review fixes.
//
// One isotropic axis-aligned Gaussian at origin (scale = (1, 1, 1), albedo = 0,
// optical_thickness chosen from sigma_multiplier). Orthographic camera at (0, 0, -5)
// looking along +Z. Viewport spans [-3, 3] x [-3, 3] in world units so the 3σ
// envelope fits cleanly. Constant white env (assets/environment_maps/white_constant.hdr).
//
// Why this scene exists: the pure-absorber, single-primitive setup has a closed-form
// per-pixel intensity:
//
//   tau(d) = optical_thickness / (2 * pi) * exp(-d^2 / 2)
//
// where d = sqrt(px^2 + py^2) is the perpendicular distance from the pixel's view ray
// to the Gaussian center. Two hypotheses for the rendered intensity are compared
// in tools/refs/single_gaussian_analytic.py:
//
//   H_analog : exp(-tau(d)) * env       (analog free-flight estimator)
//   H_double : exp(-2*tau(d)) * env     (current code's escape branch double-counts
//                                         transmittance, predicted by Finding 1)
//
// If the rendered output sits closer to H_double the exp(-tau) factor in raygen.cuh
// is a bug; if it sits on H_analog the factor is correct.
//
// sigma_multiplier maps directly to the primitive's peak extinction (no PLY scaling).
// The PLY-style bridge optical_thickness = sigma_peak * (2*pi)^{3/2} * prod(scale)
// is applied so the primitive's density convention matches the rest of the renderer.
thesis::host::utils::Result<MultiViewTestScene> single_gaussian_validation(
    float sigma_multiplier);

// Rung-2 distinct-position test: two isotropic Gaussians (scale=1, albedo=0) at
// DISTINCT positions so a camera ray pierces both at different t_hit and different
// perpendicular distances. Same camera as single_gaussian_validation. This is the
// minimal scene with the cloud's defining property (per-ray accumulation across
// primitives entered at different points) — coincident-stack tests collapse it away.
thesis::host::utils::Result<MultiViewTestScene> two_gaussian_validation(
    float sigma_multiplier);

// Overlap-ladder clusters toward the cloud. Mode selected by env SG_CLUSTER_MODE
// ∈ {n5, stress, traits}; "stress" reads SG_STRESS_K. Deterministic layouts,
// mirrored in tools/refs/render_cluster_via_prb.py. sigma_multiplier is ignored
// (per-mode masses are hardcoded so both sides match exactly).
thesis::host::utils::Result<MultiViewTestScene> cluster_validation(
    float sigma_multiplier);

}  // namespace thesis::test::scenes
