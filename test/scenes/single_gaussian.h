#pragma once

#include "scenes/cloud_validation.h"  // for MultiViewTestScene + CameraView

#include "thesis/host/utils/result.h"

namespace thesis::test::scenes {

// Verification scene for the systematic-review fixes.
//
// One isotropic axis-aligned Gaussian at origin (scale = (1, 1, 1), albedo = 0,
// optical_thickness chosen from sigma_multiplier). Orthographic camera at (0, 0, -5)
// looking along +Z. Viewport spans [-3, 3] x [-3, 3] in world units so the 3σ
// envelope fits cleanly. Constant white env (assets/white_constant.hdr).
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

}  // namespace thesis::test::scenes
