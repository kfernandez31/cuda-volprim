"""Cloud NEE-vs-analog disagreement test on the LATEST Gabor Fields volprim_prb.

Replicates the +156% cloud config (meadow env, sigma 7.5, albedo 0.9, HG g=0.85, max_depth 128, box
filter, cam 0). Reuses the EXACT scene the validated old harness builds (the C++ ellipsoids shape decodes
the PLY correctly), swapping in the Gabor volprim_prb integrator. One tiny extra shim: alias the attribute
'opacities' -> 'sigma_t' (Gabor renamed it) so Gabor's lookups resolve against the correctly-loaded shape.
The 'omega' (Gabor frequency) attribute is absent -> defaults to 0, which the gaussian kernel ignores.

Self-validation: Gabor-ANALOG mean must match the known unbiased value (~0.32). If it does, the setup is
sound and the Gabor-NEE mean is trustworthy: ~0.32 => the +156% bias is FIXED; ~0.82 => it persists.

Env: SG_NEE (1/0), SG_SPP (default 256), SG_SEED, SG_SIGMA (7.5), SG_ALBEDO (0.9), SG_HG_G (0.85);
CLOUD_DIR (cloud scene package), MEADOW_HDR (env map for SG_ENV=meadow).
Run under with_pip_gabor.sh (set VOLPRIM_DIR + CLOUD_DIR).
"""
import os, sys
from os.path import join
import numpy as np
import mitsuba as mi
mi.set_variant("cuda_ad_rgb")

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gabor_bootstrap  # is_ellipsoids / dr.freeze / Ray3f.mask / quat_to_matrix shims
import drjit as dr
from mitsuba import ScalarTransform4f as T
import volprim.integrators.volprim_prb as vpprb

# deterministic selection (as in the furnace harness / Jorge's render_asset.py default)
_orig_pt = vpprb.primitive_tracing
def _pt_det(*a, **k):
    k.setdefault("stochastic_selection_criteria", "deterministic")
    k.setdefault("orientation_selection", "deterministic")
    return _orig_pt(*a, **k)
vpprb.primitive_tracing = _pt_det

# --- alias shim: 'opacities' -> 'sigma_t' on Shape and ShapePtr (only when opacities is absent). The
# cloud PLY exposes the optical-thickness attribute as 'sigma_t'; Gabor looks it up as 'opacities'. Same
# underlying data, so this is a pure name alias (validated by the analog control landing on ~0.32). ---
def _install_alias(cls):
    _has, _e1 = cls.has_attribute, cls.eval_attribute_1
    def has_attribute(self, name, active=True):
        if name == "opacities":
            return _has(self, "sigma_t", active)
        return _has(self, name, active)
    def eval_attribute_1(self, name, si, active=True):
        if name == "opacities":
            name = "sigma_t"
        return _e1(self, name, si, active)
    cls.has_attribute = has_attribute
    cls.eval_attribute_1 = eval_attribute_1
for _c in (mi.Shape, mi.ShapePtr):
    _install_alias(_c)

# CLOUD_DIR must point to the cloud scene package (containing __init__.py + data/root.primitives_pyr0.ply).
CLOUD = os.environ.get("CLOUD_DIR")
if not CLOUD:
    sys.exit("Set CLOUD_DIR to the cloud scene package dir (with __init__.py + data/root.primitives_pyr0.ply)")
sys.path.insert(0, CLOUD)
import __init__ as cs

SIGMA = float(os.environ.get("SG_SIGMA", "7.5"))
SPP = int(os.environ.get("SG_SPP", "256"))
SEED = int(os.environ.get("SG_SEED", "0"))
ALBEDO = float(os.environ.get("SG_ALBEDO", "0.9"))
MAX_DEPTH = int(os.environ.get("SG_MAX_DEPTH", "128"))
HG = float(os.environ.get("SG_HG_G", "0.85"))
USE_NEE = os.environ.get("SG_NEE", "0") == "1"

d = {"type": "scene"}
d.update(cs.OBJECTS); d.update(cs.EMITTERS)
d.pop("resources", None)
d["primitives_pyr0"].pop("extent_adaptive_clamping", None)
d["primitives_pyr0"]["filename"] = join(CLOUD, "data/root.primitives_pyr0.ply")
# env: meadow (the +156% config, world-space roty90) or white_constant (isolation test)
if os.environ.get("SG_ENV", "meadow") == "white_constant":
    d["environment"] = {"type": "constant", "radiance": {"type": "uniform", "value": 1.0}}
else:
    _hdr = os.environ.get("MEADOW_HDR")
    if not _hdr:
        sys.exit("Set MEADOW_HDR to an environment-map .hdr for SG_ENV=meadow (or use SG_ENV=white_constant)")
    d["environment"] = {"type": "envmap",
                        "filename": _hdr,
                        "to_world": T().rotate(axis=[0, 1, 0], angle=float(os.environ.get("SG_ENV_ROTY", "90")))}
d["integrator"] = {"type": "volprim_prb", "max_depth": MAX_DEPTH,
                   "kernel_type": "gaussian", "solver_type": "bisection", "use_nee": USE_NEE,
                   "max_overlaps": int(os.environ.get("SG_MAX_OVERLAPS", "32")),
                   "kernel_full_range": os.environ.get("SG_KFR", "0") == "1",
                   "kernel_normalized": os.environ.get("SG_KNORM", "1") == "1"}
if HG != 0.0:
    d["integrator"]["phasefunction"] = {"type": "hg", "g": HG}
cam = cs.SENSORS["cam_0000"].copy(); cam.pop("resources", None)
cam = {**cam, "film": {**cam["film"], "rfilter": {"type": "box"}}}
d["cam_0000"] = cam

scene = mi.load_dict(d)
params = mi.traverse(scene)
params["primitives_pyr0.sigma_t"] = params["primitives_pyr0.sigma_t"] * SIGMA
params["primitives_pyr0.albedo"] = params["primitives_pyr0.albedo"] * 0.0 + ALBEDO
params.update()

arm = "gabor_nee" if USE_NEE else "gabor_analog"
env = os.environ.get("SG_ENV", "meadow")
img = mi.render(scene, sensor=scene.sensors()[0], spp=SPP, seed=SEED); dr.eval(img); dr.sync_thread()
arr = np.array(img).astype(np.float32)
print(f"RESULT arm={arm} env={env} spp={SPP} seed={SEED} sigma={SIGMA} albedo={ALBEDO} hg={HG} "
      f"mean={arr.mean():.4f} min={arr.min():.4f} max={arr.max():.4f}")

# EXR dump for pixelwise cross-renderer GT agreement (mirrors gabor_furnace.py). The camera IS the
# validated showcase cam_0000 (box filter), so these EXRs are pixel-aligned with our test_runner renders.
outdir = os.environ.get("SG_OUTDIR")
if outdir:
    os.makedirs(outdir, exist_ok=True)
    mi.Bitmap(img).write(join(outdir, f"{arm}_{env}_spp{SPP}_seed{SEED}.exr"))
