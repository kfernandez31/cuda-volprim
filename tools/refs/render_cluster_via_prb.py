"""
Mitsuba volprim_prb reference for the overlap-ladder clusters. MUST mirror
test/scenes/single_gaussian.cpp :: cluster_validation EXACTLY (same deterministic
layouts, same per-mode masses, same camera). Mode via env SG_CLUSTER_MODE.

  n5     : 5 isotropic overlapping absorbers
  stress : K collinear-in-z absorbers (env SG_STRESS_K, default 80), total mass 10
  traits : 8 anisotropic + Z-rotated + small-scale + varied-sigma absorbers

Run:
  SG_CLUSTER_MODE=n5 SG_SHAPE=ellipsoids SG_SPP=16384 \
    tools/refs/with_jorge_mitsuba.sh tools/refs/.venv/bin/python \
    tools/refs/render_cluster_via_prb.py
"""
import os
import math
import numpy as np
import mitsuba as mi

mi.set_variant("cuda_ad_rgb")
import drjit as dr
from mitsuba import ScalarTransform4f as T

import volprim.integrators.volprim_prb  # noqa: F401

WIDTH = 256
HEIGHT = 256
CAMERA_DISTANCE = 6.0
SPP = int(os.environ.get("SG_SPP", "1024"))
SEED = int(os.environ.get("SG_SEED", "0"))
MODE = os.environ.get("SG_CLUSTER_MODE", "n5")

THESIS_ROOT = "/home/kacper/thesis"
OUT_DIR = os.path.join(THESIS_ROOT, "test_results/single_gauss")
os.makedirs(OUT_DIR, exist_ok=True)


def zquat(deg):
    """Forward (local->world) rotation about +Z, Mitsuba order (x,y,z,w)."""
    h = math.radians(deg) / 2.0
    return [0.0, 0.0, math.sin(h), math.cos(h)]


# Build per-mode (centers, scales, quats, masses), matching the C++ exactly.
if MODE == "n5":
    ortho_height = 8.0
    M = 2.0
    C = [[0.0, 0.0, 0.0], [0.5, 0.0, 0.3], [-0.5, 0.0, -0.3],
         [0.0, 0.5, 0.4], [0.0, -0.5, -0.4]]
    S = [[1.0, 1.0, 1.0]] * 5
    Q = [[0.0, 0.0, 0.0, 1.0]] * 5
    Mass = [M] * 5
elif MODE == "stress":
    ortho_height = 6.0
    K = int(os.environ.get("SG_STRESS_K", "80"))
    Z0 = 1.5
    total_mass = 10.0
    M = total_mass / K
    C = [[0.0, 0.0, (0.0 if K == 1 else -Z0 + 2.0 * Z0 * k / (K - 1))] for k in range(K)]
    S = [[1.0, 1.0, 1.0]] * K
    Q = [[0.0, 0.0, 0.0, 1.0]] * K
    Mass = [M] * K
    print(f"stress: K={K}  M_per_prim={M:.5f}")
elif MODE == "traits":
    ortho_height = 4.0
    # (center, scale, zrot_deg, M) — identical to the C++ specs[] table.
    specs = [
        ([0.00, 0.00, 0.00], [0.50, 0.30, 0.40],   0.0, 2.0),
        ([0.40, 0.10, 0.20], [0.40, 0.50, 0.30],  30.0, 1.5),
        ([-0.35, 0.20, -0.20], [0.30, 0.40, 0.50], -45.0, 2.5),
        ([0.10, -0.40, 0.30], [0.50, 0.35, 0.30],  60.0, 1.8),
        ([-0.20, -0.30, -0.30], [0.45, 0.30, 0.40], -20.0, 2.2),
        ([0.30, 0.35, -0.25], [0.35, 0.45, 0.30],  15.0, 1.6),
        ([-0.40, -0.10, 0.35], [0.40, 0.40, 0.50],  80.0, 2.0),
        ([0.15, 0.40, 0.10], [0.30, 0.50, 0.35], -60.0, 1.9),
    ]
    C = [s[0] for s in specs]
    S = [s[1] for s in specs]
    Q = [zquat(s[2]) for s in specs]
    Mass = [s[3] for s in specs]
else:
    raise SystemExit(f"unknown SG_CLUSTER_MODE: {MODE}")

N = len(C)
centers = mi.TensorXf(np.array(C, dtype=np.float32).ravel(), shape=(N, 3))
scales = mi.TensorXf(np.array(S, dtype=np.float32).ravel(), shape=(N, 3))
quaternions = mi.TensorXf(np.array(Q, dtype=np.float32).ravel(), shape=(N, 4))
sigma_t = mi.TensorXf(np.array([[m] for m in Mass], dtype=np.float32).ravel(), shape=(N, 1))
# SG_ALBEDO drives the scattering campaign (mirrors test/scenes/single_gaussian.cpp).
# use_nee stays False below (analog = the trustworthy, energy-conserving reference;
# volprim_prb's NEE path fails the furnace test by +6.5% — see FINDINGS §8.1).
ALBEDO = float(os.environ.get("SG_ALBEDO", "0.0"))
albedo = mi.TensorXf(np.full((N, 3), ALBEDO, dtype=np.float32).ravel(), shape=(N, 3))

cam_to_world = T().look_at(
    origin=[0, 0, -CAMERA_DISTANCE], target=[0, 0, 0], up=[0, 1, 0],
) @ T().scale([ortho_height / 2.0] * 3)

scene_dict = {
    "type": "scene",
    "integrator": {
        "type": "volprim_prb",
        "max_depth": int(os.environ.get("SG_MAX_DEPTH", "32")),
        "kernel_type": "gaussian",
        "solver_type": "bisection", "use_nee": False,
    },
    "primitive": {
        **({"type": "ellipsoids"}
           if os.environ.get("SG_SHAPE") == "ellipsoids"
           else {"type": "ellipsoidsmesh", "shell": os.environ.get("SG_SHELL", "uv_sphere")}),
        "centers": centers, "scales": scales, "quaternions": quaternions,
        "sigma_t": sigma_t, "albedo": albedo, "extent": 3.0,
    },
    "environment": {"type": "constant", "radiance": {"type": "uniform", "value": 1.0}},
    "sensor": {
        "type": "orthographic", "near_clip": 0.01, "far_clip": 100.0,
        "film": {"type": "hdrfilm", "width": WIDTH, "height": HEIGHT,
                 "pixel_format": "rgb", "component_format": "float32",
                 "filter": {"type": os.environ.get("SG_RFILTER", "gaussian")}},
        "sampler": {"type": "independent"}, "to_world": cam_to_world,
    },
}

scene = mi.load_dict(scene_dict)
print(f"Rendering cluster mode={MODE} N={N} via volprim_prb  "
      f"shape={'ellipsoids' if os.environ.get('SG_SHAPE')=='ellipsoids' else 'mesh'}  spp={SPP} seed={SEED}")
img = mi.render(scene, sensor=scene.sensors()[0], spp=SPP, seed=SEED)
dr.sync_thread()

_kt = f"_K{os.environ.get('SG_STRESS_K','80')}" if MODE == "stress" else ""
_st = f"_seed{SEED}" if SEED != 0 else ""
_at = f"_alb{ALBEDO:.2f}" if ALBEDO != 0.0 else ""
out = os.path.join(OUT_DIR, f"mitsuba_cluster_{MODE}{_kt}{_at}_prb_spp{SPP}{_st}.exr")
mi.Bitmap(img).write(out)
arr = np.array(img).astype(np.float32)
print(f"wrote {out}  mean={arr.mean():.4f}  min={arr.min():.4f}  max={arr.max():.4f}")
