#!/usr/bin/env python3
"""Render an arbitrary DSYG Gaussian asset PLY through Jorge's volprim_prb integrator,
with a CUSTOM perspective camera matching our CUDA `asset_validation` scene — so the two
renderers can be diffed pixel-for-pixel (quality) and timed (perf).

Unlike render_cloud_prb_absorption.py (hard-wired to assets/models/cloud/'s scene wrapper), this
builds the volprim scene from scratch around any PLY, so it works on the raw downloaded
assets (wdas8_gauss, embergen_gauss, ...).

Camera matches host/params/camera.h createPerspective: vertical FOV in degrees, look_at
(origin, target=0, up=+y). SG_VIEW picks the axis like the CUDA asset_validation scene.

Env vars (mirror the CUDA side):
  SG_PLY      asset PLY (Jorge's native root.primitives_pyr0.ply — has opacities_0 etc.)
  SG_SIGMA    density scale (= CUDA --sigma-multiplier)       default 10
  SG_ALBEDO   uniform albedo override (>0 = scattering)        default 0.9
  SG_SPP      samples per pixel                                default 64
  SG_NEE      0 = analog (trustworthy ref), 1 = NEE variant    default 0
  SG_HG_G     HG anisotropy                                    default 0.85
  SG_MAX_DEPTH max path depth                                  default 128
  SG_RES      square resolution                                default 512
  SG_DIST / SG_FOV / SG_VIEW   camera (match asset_validation) default 3.5 / 40 / negz
  SG_ENV      white_constant | meadow                          default white_constant
  OUT         output EXR path                          default /tmp/mits_asset/0000.exr
"""
import os, sys, time
from pathlib import Path
import numpy as np
import mitsuba as mi

mi.set_variant("cuda_ad_rgb")

# volprim was written against an older Mitsuba API (put_parameter -> put_value in 3.8).
if hasattr(mi, "TraversalCallback") and not hasattr(mi.TraversalCallback, "put_parameter"):
    def _put_parameter_compat(self, name, value, flags):
        return self.put_value(name, value, flags, None)
    mi.TraversalCallback.put_parameter = _put_parameter_compat

import volprim.integrators.volprim_prb  # noqa: F401 (registers integrator)
from mitsuba import ScalarTransform4f as T

PLY = os.environ["SG_PLY"]
SIGMA = float(os.environ.get("SG_SIGMA", "10"))
ALBEDO = float(os.environ.get("SG_ALBEDO", "0.9"))
SPP = int(os.environ.get("SG_SPP", "64"))
USE_NEE = os.environ.get("SG_NEE", "0") == "1"
HG_G = float(os.environ.get("SG_HG_G", "0.85"))
MAX_DEPTH = int(os.environ.get("SG_MAX_DEPTH", "128"))
RES = int(os.environ.get("SG_RES", "512"))
DIST = float(os.environ.get("SG_DIST", "3.5"))
FOV = float(os.environ.get("SG_FOV", "40"))
VIEW = os.environ.get("SG_VIEW", "negz")
SG_ENV = os.environ.get("SG_ENV", "white_constant")
OUT = Path(os.environ.get("OUT", "/tmp/mits_asset/0000.exr"))

# Camera origin/up per SG_VIEW (mirrors test/scenes/cloud_validation.cpp asset_validation).
views = {
    "posz": ([0, 0, DIST], [0, 1, 0]), "negz": ([0, 0, -DIST], [0, 1, 0]),
    "posx": ([DIST, 0, 0], [0, 1, 0]), "negx": ([-DIST, 0, 0], [0, 1, 0]),
    "posy": ([0, DIST, 0], [0, 0, 1]), "negy": ([0, -DIST, 0], [0, 0, 1]),
    "diag": ([DIST * 0.6, DIST * 0.5, -DIST * 0.6], [0, 1, 0]),
}
origin, up = views.get(VIEW, ([0, 0, -DIST], [0, 1, 0]))

scene_dict = {
    "type": "scene",
    "integrator": {
        "type": "volprim_prb", "max_depth": MAX_DEPTH, "use_nee": USE_NEE,
        "phasefunction": {"type": "hg", "g": HG_G},
    },
    "primitives_pyr0": {
        "type": "ellipsoids", "extent": 3.0, "filename": PLY,
    },
    "sensor": {
        "type": "perspective", "fov": FOV, "fov_axis": "y",
        "to_world": T().look_at(origin=origin, target=[0, 0, 0], up=up),
        "film": {"type": "hdrfilm", "width": RES, "height": RES, "rfilter": {"type": "box"}},
        "sampler": {"type": "independent", "sample_count": SPP},
    },
}
if SG_ENV == "meadow":
    scene_dict["environment"] = {
        "type": "envmap", "filename": "assets/environment_maps/meadow_2_4k.hdr",
        "to_world": T().rotate(axis=[0, 1, 0], angle=float(os.environ.get("SG_ENV_ROTY", "90"))),
    }
else:
    scene_dict["environment"] = {"type": "constant", "radiance": {"type": "uniform", "value": 1.0}}

mode = "NEE" if USE_NEE else "ANALOG"
print(f"volprim_prb {mode}: ply={Path(PLY).name} sigma={SIGMA} albedo={ALBEDO} g={HG_G} "
      f"depth={MAX_DEPTH} spp={SPP} res={RES} view={VIEW} env={SG_ENV}", flush=True)

scene = mi.load_dict(scene_dict)
params = mi.traverse(scene)
# PLY density property renamed opacities_0 -> sigma_t_0 so volprim_prb finds sigma_t.
params["primitives_pyr0.sigma_t"] = params["primitives_pyr0.sigma_t"] * SIGMA
n = len(params["primitives_pyr0.sigma_t"])
params["primitives_pyr0.albedo"] = np.full(n * 3, ALBEDO, dtype=np.float32)
params.update()

# Warm-up (JIT compile) then timed render, so the time is the render kernel, not compilation.
import drjit as dr
sensor = scene.sensors()[0]
_ = mi.render(scene, sensor=sensor, spp=1, seed=123)
dr.sync_thread()
t0 = time.time()
img = mi.render(scene, sensor=sensor, spp=SPP, seed=0)
dr.sync_thread()
elapsed = time.time() - t0

OUT.parent.mkdir(parents=True, exist_ok=True)
mi.util.write_bitmap(str(OUT), img)
print(f"RENDER_TIME_S {elapsed:.3f}  -> {OUT}", flush=True)
