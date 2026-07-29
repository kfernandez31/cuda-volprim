#!/usr/bin/env python3
"""Render the voxelized cloud from the EXACT cloud scene (cam_0000 + meadow env), swapping ONLY the
medium: the Gaussian 'ellipsoids' object is replaced by a heterogeneous/gridvolume of the SAME density
field (our convention). Scattering (albedo 0.9, HG g=0.85, sigmat 7.5) so the cloud is lit, not a black
silhouette. This guarantees the same camera as our g1 / refs renders.

Run: experiments/mitsuba-reference/with_jorge_mitsuba.sh experiments/mitsuba-reference/.venv/bin/python -u experiments/mitsuba-reference/voxel_cloud_scene.py [VOX] [SPP]
"""
import os, sys
import numpy as np
from plyfile import PlyData
import mitsuba as mi
mi.set_variant("cuda_ad_rgb")
import volprim.integrators.volprim_prb  # noqa (registers volprim; not used, but matches scene import)
T = mi.ScalarTransform4f

VOX = int(sys.argv[1]) if len(sys.argv) > 1 else 200
SPP = int(sys.argv[2]) if len(sys.argv) > 2 else 32
CLAMP = 80.0
ENV = "/home/kacper/thesis/assets/environment_maps/meadow_2_4k.hdr"

# --- world bbox of the cloud (same frame as the Gaussians) ---
v = PlyData.read("assets/models/cloud/root.primitives_pyr0.ply")["vertex"]
mu = np.stack([v["x"], v["y"], v["z"]], -1).astype(np.float64)
s = np.exp(np.stack([v["scale_0"], v["scale_1"], v["scale_2"]], -1)).astype(np.float64)
lo = (mu - 3 * s.max(1, keepdims=True)).min(0)
hi = (mu + 3 * s.max(1, keepdims=True)).max(0)
size = hi - lo

cache = f"/tmp/cloud_grid_{VOX}.npy"
if not os.path.exists(cache):
    raise SystemExit(f"grid {cache} not found - run voxel_cloud.py {VOX} first to build it")
grid = np.load(cache)
grid = np.minimum(grid, CLAMP).astype(np.float32)
# Mitsuba gridvolume indexes data as [Z,Y,X]; our grid is [X,Y,Z] -> transpose.
grid_m = np.ascontiguousarray(grid.transpose(2, 1, 0))
print(f"grid {grid.shape} -> mitsuba {grid_m.shape}  max {grid.max():.1f} mean {grid.mean():.3f}", flush=True)

# --- load the real cloud scene, swap medium + integrator + env ---
sys.path.insert(0, "assets/models/cloud")
import __init__ as cloud  # noqa

scene_dict = {"type": "scene"}
scene_dict.update({k: vv for k, vv in cloud.OBJECTS.items() if k not in ("resources", "primitives_pyr0")})
scene_dict.update({k: vv for k, vv in cloud.SENSORS.items()
                   if k == "cam_0000"})                       # only cam_0000
# integrator: heterogeneous-grid path tracer (not the Gaussian volprim_prb)
scene_dict["integrator"] = {"type": "prbvolpath", "max_depth": 64}
# the voxel medium, placed at the cloud's world bbox
scene_dict["cloud_grid"] = {
    "type": "cube", "bsdf": {"type": "null"},
    "to_world": T().translate(((lo + hi) / 2).tolist()).scale((size / 2).tolist()),
    "interior": {
        "type": "heterogeneous",
        "sigma_t": {"type": "gridvolume", "data": mi.TensorXf(grid_m),
                    "to_world": T().translate(lo.tolist()).scale(size.tolist())},
        "albedo": {"type": "uniform", "value": 0.9},
        "phase": {"type": "hg", "g": 0.85},
        "scale": 1.0,
    },
}
# meadow env (matches our g1 render: roty 90)
scene_dict["environment"] = {"type": "envmap", "filename": ENV,
                             "to_world": T().rotate(axis=[0, 1, 0], angle=90.0)}

scene = mi.load_dict(scene_dict)
print(f"rendering cam_0000, {SPP} spp, scattering+meadow ...", flush=True)
img = mi.render(scene, spp=SPP)
mi.util.write_bitmap("results/campaign/voxel_cloud_lit.exr", img)
mi.util.write_bitmap("results/campaign/voxel_cloud_lit.png", img)
print(f"render mean {np.array(img).mean():.4f}  wrote results/campaign/voxel_cloud_lit.{{exr,png}}", flush=True)
