"""Measure Mitsuba's JIT/startup overhead (task #96): import + scene build + first
render (Dr.Jit trace + kernel compile/cache-load + launch) vs steady-state renders.

Same cloud PLY/camera/integrator as render_ply_via_mitsuba.py, low spp (16) so the
steady-state frame is small and the first-render delta is dominated by JIT work.

Run (3 repeats = 3 processes, warm disk cache = the typical-session number):
    for i in 1 2 3; do experiments/mitsuba-reference/with_jorge_mitsuba.sh \
        experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/jit_overhead_timing.py; done
"""
import os
import sys
import time

t0 = time.perf_counter()
import mitsuba as mi

mi.set_variant("cuda_ad_rgb")
import drjit as dr
t_import = time.perf_counter() - t0

from mitsuba import ScalarTransform4f as T
import volprim.integrators.volprim_tomography  # noqa: F401
import volprim.integrators.volprim_prb  # noqa: F401

# INTEGRATOR=prb -> the full scattering integrator (the one we benchmark against;
# its JIT trace is far larger than tomography's). Default: tomography.
USE_PRB = os.environ.get("INTEGRATOR", "tomography") == "prb"

THESIS_ROOT = "/home/kacper/thesis"
sys.path.insert(0, os.path.join(THESIS_ROOT, "assets/models/cloud"))
import __init__ as cloud_scene

PLY = os.path.join(THESIS_ROOT, "assets/models/cloud/root.primitives_pyr0.ply")
SPP = 16

t1 = time.perf_counter()
cam = cloud_scene.SENSORS["cam_0000"]
integrator = ({"type": "volprim_prb", "max_depth": 64, "use_nee": True}
              if USE_PRB else {"type": "volprim_tomography", "max_depth": 64})
emitter = ({"type": "envmap",
            "filename": os.path.join(THESIS_ROOT, "assets/environment_maps/meadow_2_4k.hdr")}
           if USE_PRB else
           {"type": "constant", "radiance": {"type": "rgb", "value": 1.0}})
scene = mi.load_dict({
    "type": "scene",
    "integrator": integrator,
    "sensor": {
        "type": "perspective",
        "fov": cam["fov"] if isinstance(cam, dict) and "fov" in cam else 39.0,
        "to_world": cam["to_world"] if isinstance(cam, dict) else cam,
        "film": {"type": "hdrfilm", "width": 256, "height": 256},
        "sampler": {"type": "independent", "sample_count": SPP},
    },
    "primitives": {
        "type": "ellipsoids",
        "filename": PLY,
        "scale_factor": 1.0,
    },
    "emitter": emitter,
})
print(f"RESULT integrator: {'volprim_prb+meadow' if USE_PRB else 'volprim_tomography+constant'}")
t_scene = time.perf_counter() - t1

times = []
for i in range(3):
    t = time.perf_counter()
    img = mi.render(scene, spp=SPP, seed=i)
    dr.eval(img)
    dr.sync_thread()
    times.append(time.perf_counter() - t)

print(f"RESULT import+variant: {t_import:.3f} s")
print(f"RESULT scene build:    {t_scene:.3f} s")
print(f"RESULT render #1 (JIT+launch): {times[0]:.3f} s")
print(f"RESULT render #2 (steady):     {times[1]:.3f} s")
print(f"RESULT render #3 (steady):     {times[2]:.3f} s")
print(f"RESULT JIT/startup delta (r1 - r3): {times[0]-times[2]:.3f} s")
print(f"RESULT total time-to-first-image: {t_import + t_scene + times[0]:.3f} s")
