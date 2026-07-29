"""Steady-state timing for the corrected-NEE fork on the showcase cloud (Task 3).
Builds the scene ONCE, does 1 warmup render (JIT), then N timed mi.render+sync repeats -> per-repeat
render seconds. Same scene as gabor_cloud.py (meadow, sigma7.5, albedo0.9, HG0.85, cam_0000, box).

Env: SG_SPP (256), SG_SEED (0), SG_REPEATS (5), SG_WARMUP_SPP (16); CLOUD_DIR, MEADOW_HDR, VOLPRIM_DIR.
Prints one `TIMING_S <t>` line per timed repeat. Run under with_pip_gabor.sh at LOCKED 350 W for a valid
measurement (the driver enforces the power/clock gate; this script just renders + times)."""
import os, sys, time
from os.path import join
import numpy as np
import mitsuba as mi
mi.set_variant("cuda_ad_rgb")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gabor_bootstrap
import drjit as dr
from mitsuba import ScalarTransform4f as T
import volprim.integrators.volprim_prb as vpprb

_orig_pt = vpprb.primitive_tracing
def _pt_det(*a, **k):
    k.setdefault("stochastic_selection_criteria", "deterministic")
    k.setdefault("orientation_selection", "deterministic")
    return _orig_pt(*a, **k)
vpprb.primitive_tracing = _pt_det

def _install_alias(cls):
    _has, _e1 = cls.has_attribute, cls.eval_attribute_1
    def has_attribute(self, name, active=True):
        return _has(self, "sigma_t", active) if name == "opacities" else _has(self, name, active)
    def eval_attribute_1(self, name, si, active=True):
        return _e1(self, "sigma_t" if name == "opacities" else name, si, active)
    cls.has_attribute = has_attribute
    cls.eval_attribute_1 = eval_attribute_1
for _c in (mi.Shape, mi.ShapePtr):
    _install_alias(_c)

CLOUD = os.environ.get("CLOUD_DIR") or sys.exit("Set CLOUD_DIR")
sys.path.insert(0, CLOUD)
import __init__ as cs

SIGMA = float(os.environ.get("SG_SIGMA", "7.5"))
SPP = int(os.environ.get("SG_SPP", "256"))
SEED = int(os.environ.get("SG_SEED", "0"))
ALBEDO = float(os.environ.get("SG_ALBEDO", "0.9"))
MAX_DEPTH = int(os.environ.get("SG_MAX_DEPTH", "128"))
HG = float(os.environ.get("SG_HG_G", "0.85"))
REPEATS = int(os.environ.get("SG_REPEATS", "5"))
WARMUP_SPP = int(os.environ.get("SG_WARMUP_SPP", "16"))

d = {"type": "scene"}
d.update(cs.OBJECTS); d.update(cs.EMITTERS)
d.pop("resources", None)
d["primitives_pyr0"].pop("extent_adaptive_clamping", None)
d["primitives_pyr0"]["filename"] = join(CLOUD, "data/root.primitives_pyr0.ply")
_hdr = os.environ.get("MEADOW_HDR") or sys.exit("Set MEADOW_HDR")
d["environment"] = {"type": "envmap", "filename": _hdr,
                    "to_world": T().rotate(axis=[0, 1, 0], angle=float(os.environ.get("SG_ENV_ROTY", "90")))}
d["integrator"] = {"type": "volprim_prb", "max_depth": MAX_DEPTH, "kernel_type": "gaussian",
                   "solver_type": "bisection", "use_nee": True,
                   "max_overlaps": int(os.environ.get("SG_MAX_OVERLAPS", "32")),
                   "kernel_full_range": False, "kernel_normalized": True}
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
sensor = scene.sensors()[0]

print(f"[gabor_timing] volprim={getattr(vpprb,'__file__','?')} spp={SPP} repeats={REPEATS} "
      f"warmup_spp={WARMUP_SPP}", flush=True)
# warmup: compiles kernels + spins caches (excluded from timing)
# Timed repeats using Jorge's own benchmark pattern (generate_figures/benchmark_stochastic_selection.py):
# dr.kernel_history under KernelHistory+LaunchBlocking flags -> sum of pure GPU kernel execution time,
# excluding python/graph-tracing/host overhead entirely.
for r in range(REPEATS):
    dr.kernel_history_clear()
    dr.sync_thread()
    t0 = time.perf_counter()
    with dr.scoped_set_flag(dr.JitFlag.KernelHistory, True):
        with dr.scoped_set_flag(dr.JitFlag.LaunchBlocking, True):
            img = mi.render(scene, sensor=sensor, spp=SPP, seed=SEED); dr.eval(img); dr.sync_thread()
        history = dr.kernel_history([dr.KernelType.JIT])
    wall = time.perf_counter() - t0
    kexec_s = sum(k['execution_time'] for k in history) / 1000.0
    print(f"TIMING_KH wall_blocking_s={wall:.4f} kernel_exec_s={kexec_s:.4f} kernels={len(history)}", flush=True)
