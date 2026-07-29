"""Furnace test on the LATEST Gabor Fields volprim_prb (Jorge's rewritten NEE integrator).

Single Gaussian, albedo=1, constant
env=1 -> every pixel must equal 1.0 by radiative equilibrium at ANY spp (convention-robust GT: albedo=1
means zero absorption, so kernel_normalized/full_range/extent/sigma_t cannot change the equilibrium
mean; they only change variance). Analog (use_nee=0) is the control that must return 1.0; NEE is under
test.

Env:
  SG_ALBEDO (default 1.0), SG_SIGMA (default 6), SG_NEE (1/0), SG_MAX_DEPTH (256; -1 = unlimited),
  SG_SHAPE (ellipsoids|ellipsoidsmesh, default ellipsoids), SG_SPPS, SG_SEEDS, SG_ARM, SG_CSV,
  SG_KERNEL (gaussian), SG_SOLVER (bisection), SG_SOLVER_ITERS (unset = plugin default 4),
  SG_VARIANT (default cuda_ad_rgb; e.g. llvm_ad_rgb for the CPU-backend A/B),
  SG_ENVMAP_FILE (unset = constant emitter 1.0; set to an all-white lat-long EXR to exercise the
  envmap emitter-sampling path instead — GT is still exactly 1.0),
  SG_OUTDIR (unset = no EXR dumps; set to a directory to save each render as fp32 EXR).
Run under with_pip_gabor.sh (sets VOLPRIM_DIR -> the Gabor volprim checkout on PYTHONPATH). E.g.:
  VOLPRIM_DIR=/path/to/GaborVolumes ./with_pip_gabor.sh python gabor_furnace.py
"""
import os, sys, math
import numpy as np
import mitsuba as mi
mi.set_variant(os.environ.get("SG_VARIANT", "cuda_ad_rgb"))

# apply the two faithful shims (is_ellipsoids, dr.freeze) BEFORE importing volprim
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gabor_bootstrap  # noqa: F401
import drjit as dr
from mitsuba import ScalarTransform4f as T
import volprim.integrators.volprim_prb as vpprb

# Force DETERMINISTIC orientation/selection in primitive_tracing. Plain volprim_prb never passes a
# select_mask (only the hierarchical/radiance-field integrator does), so the shipped 'uniform' default
# would call select_mask=None. 'deterministic' is the mode Jorge's own render_asset.py defaults to and
# the correct one for single-orientation scattering media (traces ray.mask=255 = all slabs). It does NOT
# touch the NEE/MIS estimator under test.
_orig_pt = vpprb.primitive_tracing
def _pt_det(*a, **k):
    k.setdefault("stochastic_selection_criteria", "deterministic")
    k.setdefault("orientation_selection", "deterministic")
    return _orig_pt(*a, **k)
vpprb.primitive_tracing = _pt_det

WIDTH = HEIGHT = 256
ORTHO_HEIGHT = 6.0
CAMERA_DISTANCE = 5.0

SIGMA = float(os.environ.get("SG_SIGMA", "6.0"))
# SG_NPRIM=N stacks N identical overlapping Gaussians at the origin. Furnace GT is STILL exactly 1.0
# (albedo=1 conserves energy regardless of density), so any droop vs N directly measures OVERLAP-handling
# energy loss. sigma_t is split 1/N per prim so total optical thickness matches the N=1 case.
# SG_SPACING=d (world units, default 0) spreads the N Gaussians ALONG THE VIEW AXIS (+z), centred on the
# origin, each at FULL sigma_t (no 1/N split). GT is still exactly 1.0 (albedo=1). This activates the
# shadow-transmittance MARCH (primitives entered from outside, ahead) that co-located stacks never do.
N = int(os.environ.get("SG_NPRIM", "1"))
SPACING = float(os.environ.get("SG_SPACING", "0"))
if os.environ.get("SG_TRANSFORMED") == "1":
    import math as _m
    _h = _m.radians(30.0) / 2.0
    _scale, _quat = [1.0, 0.5, 0.75], [0.0, _m.sin(_h), 0.0, _m.cos(_h)]  # (x,y,z,w), roty30
else:
    _scale, _quat = [1.0, 1.0, 1.0], [0.0, 0.0, 0.0, 1.0]
_ctr = np.zeros((N, 3), np.float32)
if SPACING > 0:
    _ctr[:, 2] = (np.arange(N) - (N - 1) / 2.0) * SPACING     # chain along the view axis
centers = mi.TensorXf(_ctr.ravel(), shape=(N, 3))
scales = mi.TensorXf(np.tile(_scale, (N, 1)).astype(np.float32).ravel(), shape=(N, 3))
quaternions = mi.TensorXf(np.tile(_quat, (N, 1)).astype(np.float32).ravel(), shape=(N, 4))
_sig_per = SIGMA if SPACING > 0 else SIGMA / N
sigma_t = mi.TensorXf(np.full((N, 1), _sig_per, np.float32).ravel(), shape=(N, 1))
_alb = [float(x) for x in os.environ.get("SG_ALBEDO", "1.0").split(",")]
RGB = _alb if len(_alb) >= 3 else _alb * 3
albedo = mi.TensorXf(np.tile(RGB[:3], (N, 1)).astype(np.float32).ravel(), shape=(N, 3))

cam_to_world = T().look_at(origin=[0, 0, -CAMERA_DISTANCE], target=[0, 0, 0], up=[0, 1, 0]) \
    @ T().scale([ORTHO_HEIGHT / 2.0] * 3)

integ = {
    "type": "volprim_prb",
    "max_depth": int(os.environ.get("SG_MAX_DEPTH", "256")),
    "kernel_type": os.environ.get("SG_KERNEL", "gaussian"),
    "solver_type": os.environ.get("SG_SOLVER", "bisection"),
    "use_nee": os.environ.get("SG_NEE", "1") == "1",
}
if os.environ.get("SG_SOLVER_ITERS"):
    integ["solver_max_iterations"] = int(os.environ["SG_SOLVER_ITERS"])
_hg = os.environ.get("SG_HG_G")
if _hg and float(_hg) != 0.0:
    integ["phasefunction"] = {"type": "hg", "g": float(_hg)}

prim = ({"type": "ellipsoids"} if os.environ.get("SG_SHAPE", "ellipsoids") == "ellipsoids"
        else {"type": "ellipsoidsmesh", "shell": "uv_sphere"})
# Gabor renamed the shape attribute sigma_t -> opacities; numeric role is identical
# (ellipsoid.opacity multiplies density to give optical depth: tr = exp(-density*opacity)).
prim.update(centers=centers, scales=scales, quaternions=quaternions,
            opacities=sigma_t, albedo=albedo, extent=float(os.environ.get("SG_EXTENT", "3.0")))

# Constant emitter (exact 1.0) by default; SG_ENVMAP_FILE swaps in the envmap plugin (2D-CDF
# emitter sampling path). With an all-white map the GT is still exactly 1.0.
if os.environ.get("SG_ENVMAP_FILE"):
    # SG_ENV_ROTY: world-space Y rotation of the envmap (default 90 = the calibrated orientation that
    # matches our CUDA renderer's u=atan2(z,x) convention; see FINDINGS 8.6).
    env_block = {"type": "envmap", "filename": os.environ["SG_ENVMAP_FILE"],
                 "to_world": T().rotate(axis=[0, 1, 0], angle=float(os.environ.get("SG_ENV_ROTY", "90")))}
else:
    env_block = {"type": "constant", "radiance": {"type": "uniform", "value": 1.0}}

scene = mi.load_dict({
    "type": "scene",
    "integrator": integ,
    "primitive": prim,
    "environment": env_block,
    "sensor": {
        "type": "orthographic", "near_clip": 0.01, "far_clip": 100.0,
        "film": {"type": "hdrfilm", "width": WIDTH, "height": HEIGHT,
                 "pixel_format": "rgb", "component_format": "float32",
                 "filter": {"type": os.environ.get("SG_RFILTER", "box")}},
        "sampler": {"type": "independent"},
        "to_world": cam_to_world,
    },
})

def centre_box(arr):
    g = arr.mean(-1); h, w = g.shape
    return float(g[h // 2 - h // 8:h // 2 + h // 8, w // 2 - w // 8:w // 2 + w // 8].mean())

arm = os.environ.get("SG_ARM", "gabor_" + ("nee" if integ["use_nee"] else "analog"))
spps = [int(x) for x in os.environ.get("SG_SPPS", os.environ.get("SG_SPP", "256")).split()]
seeds = [int(x) for x in os.environ.get("SG_SEEDS", os.environ.get("SG_SEED", "0")).split()]
outdir = os.environ.get("SG_OUTDIR")
if outdir:
    os.makedirs(outdir, exist_ok=True)

# Auditability: echo versions + the integrator's RESOLVED parameters (not just what we passed).
_integ_obj = scene.integrator()
_resolved = {k: getattr(_integ_obj, k, None) for k in
             ("max_depth", "rr_depth", "rr_nee_depth", "use_rr", "use_rr_nee", "use_nee",
              "solver_type", "solver_max_iterations", "max_overlaps")}
print(f"[gabor_furnace] mitsuba={getattr(mi,'__version__','?')} variant={mi.variant()} "
      f"volprim={getattr(vpprb,'__file__','?')}")
print(f"[gabor_furnace] resolved integrator params: {_resolved}")
print(f"[gabor_furnace] arm={arm} sigma={SIGMA} albedo={RGB} use_nee={integ['use_nee']} "
      f"max_depth_req={integ['max_depth']} shape={prim['type']} emitter="
      f"{'envmap:'+os.environ['SG_ENVMAP_FILE'] if os.environ.get('SG_ENVMAP_FILE') else 'constant(1.0)'}")

rows = []
for spp in spps:
    for seed in seeds:
        img = mi.render(scene, sensor=scene.sensors()[0], spp=spp, seed=seed)
        dr.sync_thread()
        arr = np.array(img).astype(np.float32)
        m, c = float(arr.mean()), centre_box(arr)
        print(f"RESULT arm={arm} sigma={SIGMA:.3f} spp={spp} seed={seed} mean={m:.6f} "
              f"centre={c:.6f} overcount%={100*(c-1):.3f}")
        rows.append((arm, f"{SIGMA:.3f}", spp, seed, f"{m:.6f}", f"{c:.6f}"))
        if outdir:
            mi.Bitmap(img).write(os.path.join(outdir, f"{arm}_sigma{SIGMA:g}_spp{spp}_seed{seed}.exr"))

csv_path = os.environ.get("SG_CSV")
if csv_path:
    import csv
    new = not os.path.exists(csv_path)
    with open(csv_path, "a", newline="") as f:
        w = csv.writer(f)
        if new:
            w.writerow(["arm", "sigma", "spp", "seed", "mean", "centre"])
        w.writerows(rows)
    print(f"appended {len(rows)} rows to {csv_path}")
