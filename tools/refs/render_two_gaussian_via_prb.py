"""
Render the two-Gaussian overlap validation scene through Mitsuba's volprim_prb,
to pair against CUDA's `two_gaussian_validation` scene.

This is the first scene with the cloud's defining property: a camera ray pierces
TWO primitives entered at DIFFERENT t_hit (z offset) and DIFFERENT perpendicular
distance (x offset), so it exercises distinct-position optical-depth accumulation
across the sorted entry/exit events — the multi-primitive transmittance path that
`compute_transmittance_to_env` performs on the CUDA side (now load-bearing under
ENABLE_ANALYTIC_DIRECT).

Config MUST match test/scenes/single_gaussian.cpp :: two_gaussian_validation:
  prim 0: center (-0.5, 0, -0.5), scale (1,1,1), identity quat, albedo 0
  prim 1: center (+0.5, 0, +0.5), scale (1,1,1), identity quat, albedo 0
  optical_thickness = sigma_multiplier (post-241d47b convention, no (2π)^{3/2} bridge)
  ortho camera at (0,0,-5) -> origin, up +Y, viewport [-3,3]^2, 256x256, white env=1.0

Run via:
    tools/refs/with_jorge_mitsuba.sh \
        tools/refs/.venv/bin/python tools/refs/render_two_gaussian_via_prb.py
Env: SG_SHAPE=ellipsoids (exact analytic shape, like CUDA), SG_SPP, SG_SEED.
"""
import os
import math
import numpy as np
import mitsuba as mi

mi.set_variant("cuda_ad_rgb")
import drjit as dr
from mitsuba import ScalarTransform4f as T

import volprim.integrators.volprim_prb  # noqa: F401

# These MUST match test/scenes/single_gaussian.cpp constants.
WIDTH = 256
HEIGHT = 256
ORTHO_HEIGHT = 6.0
CAMERA_DISTANCE = 5.0

SIGMA_MULTIPLIER_CUDA = 4.0
SIGMA_T_MITSUBA = SIGMA_MULTIPLIER_CUDA
SPP = int(os.environ.get("SG_SPP", "1024"))
SEED = int(os.environ.get("SG_SEED", "0"))

THESIS_ROOT = "/home/kacper/thesis"
OUT_DIR = os.path.join(THESIS_ROOT, "test_results/single_gauss")
os.makedirs(OUT_DIR, exist_ok=True)

# Two distinct-position isotropic absorbers (mirrors two_gaussian_validation).
N = 2
centers = mi.TensorXf(np.array([[-0.5, 0.0, -0.5],
                                [ 0.5, 0.0,  0.5]], dtype=np.float32).ravel(),
                      shape=(N, 3))
scales = mi.TensorXf(np.array([[1.0, 1.0, 1.0],
                               [1.0, 1.0, 1.0]], dtype=np.float32).ravel(),
                     shape=(N, 3))
# Mitsuba quaternion: (x, y, z, w); identity for both.
quaternions = mi.TensorXf(np.array([[0.0, 0.0, 0.0, 1.0],
                                    [0.0, 0.0, 0.0, 1.0]], dtype=np.float32).ravel(),
                          shape=(N, 4))
sigma_t = mi.TensorXf(np.array([[SIGMA_T_MITSUBA], [SIGMA_T_MITSUBA]], dtype=np.float32).ravel(),
                      shape=(N, 1))
# SG_ALBEDO drives the scattering campaign. use_nee stays False (analog reference).
ALBEDO = float(os.environ.get("SG_ALBEDO", "0.0"))
albedo = mi.TensorXf(np.full((N, 3), ALBEDO, dtype=np.float32).ravel(), shape=(N, 3))

cam_to_world = T().look_at(
    origin=[0, 0, -CAMERA_DISTANCE],
    target=[0, 0, 0],
    up=[0, 1, 0],
) @ T().scale([ORTHO_HEIGHT / 2.0] * 3)

scene_dict = {
    "type": "scene",
    "integrator": {
        "type": "volprim_prb",
        "max_depth": 32,
        "kernel_type": "gaussian",
        "solver_type": "bisection",
        "use_nee": False,
    },
    "primitive": {
        **({"type": "ellipsoids"}
           if os.environ.get("SG_SHAPE") == "ellipsoids"
           else {"type": "ellipsoidsmesh", "shell": os.environ.get("SG_SHELL", "uv_sphere")}),
        "centers": centers,
        "scales": scales,
        "quaternions": quaternions,
        "sigma_t": sigma_t,
        "albedo": albedo,
        "extent": 3.0,
    },
    "environment": {
        "type": "constant",
        "radiance": {"type": "uniform", "value": 1.0},
    },
    "sensor": {
        "type": "orthographic",
        "near_clip": 0.01,
        "far_clip": 100.0,
        "film": {
            "type": "hdrfilm",
            "width": WIDTH,
            "height": HEIGHT,
            "pixel_format": "rgb",
            "component_format": "float32",
            "filter": {"type": os.environ.get("SG_RFILTER", "gaussian")},
        },
        "sampler": {"type": "independent"},
        "to_world": cam_to_world,
    },
}

scene = mi.load_dict(scene_dict)
print(f"Rendering TWO Gaussians via volprim_prb  sigma_t={SIGMA_T_MITSUBA:.6f}  "
      f"shape={'ellipsoids' if os.environ.get('SG_SHAPE')=='ellipsoids' else 'mesh'}  spp={SPP}  seed={SEED}")
img = mi.render(scene, sensor=scene.sensors()[0], spp=SPP, seed=SEED)
dr.sync_thread()

_seedtag = f"_seed{SEED}" if SEED != 0 else ""
_at = f"_alb{ALBEDO:.2f}" if ALBEDO != 0.0 else ""
out = os.path.join(OUT_DIR, f"mitsuba_two_gauss_prb{_at}_M={SIGMA_T_MITSUBA:.3f}_spp{SPP}{_seedtag}.exr")
mi.Bitmap(img).write(out)
arr = np.array(img).astype(np.float32)
print(f"wrote {out}  mean={arr.mean():.4f}  max={arr.max():.4f}  min={arr.min():.4f}")
