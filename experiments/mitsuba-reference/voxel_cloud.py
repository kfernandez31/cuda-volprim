#!/usr/bin/env python3
"""Voxelize the cloud's Gaussians into a dense sigma_t grid IN OUR RENDERER'S CONVENTION, then render
that grid through Mitsuba's independent heterogeneous/gridvolume/prbvolpath path tracer. The point is a
visual sanity check: does a grid path-trace of the same density field reproduce the cloud our analytic
renderer produces?

Density convention (matches device/core/primitive.h):
  sigma_t(x) = sum_i (SIGMA_MULT * sigma_t_i) * (2*pi)^(-3/2) * prod(1/s_ij)
               * exp(-0.5 * || S_i^{-1} R_i^T (x - mu_i) ||^2)

Run: experiments/mitsuba-reference/with_jorge_mitsuba.sh experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/voxel_cloud.py
"""
import numpy as np
from plyfile import PlyData
from scipy.spatial.transform import Rotation
import mitsuba as mi
mi.set_variant("cuda_ad_rgb")
T = mi.ScalarTransform4f

import sys
SIGMA_MULT = 7.5            # cloud sigmat_scale (args.json) == our --sigma-multiplier
NORM = (2 * np.pi) ** (-1.5)
VOX = int(sys.argv[1]) if len(sys.argv) > 1 else 160   # voxels along longest bbox axis
SPP = int(sys.argv[2]) if len(sys.argv) > 2 else 16
PLY = "assets/models/cloud/root.primitives_pyr0.ply"

# --- parse Gaussians ---
v = PlyData.read(PLY)["vertex"]
mu = np.stack([v["x"], v["y"], v["z"]], -1).astype(np.float64)
s = np.exp(np.stack([v["scale_0"], v["scale_1"], v["scale_2"]], -1)).astype(np.float64)  # log->linear
quat = np.stack([v["rot_0"], v["rot_1"], v["rot_2"], v["rot_3"]], -1).astype(np.float64)  # xyzw
sigt = np.array(v["sigma_t_0"]).astype(np.float64)
Rm = Rotation.from_quat(quat).as_matrix()                  # (N,3,3), world<-local
amp = SIGMA_MULT * sigt * NORM / np.prod(s, axis=1)        # per-Gaussian peak density
N = len(mu)

# --- grid spanning the 3-sigma world bbox ---
lo = (mu - 3 * s.max(1, keepdims=True)).min(0)
hi = (mu + 3 * s.max(1, keepdims=True)).max(0)
size = hi - lo
res = np.maximum((VOX * size / size.max()).round().astype(int), 8)
vox = size / res                                            # world size of one voxel
print(f"N={N} bbox lo={lo.round(2)} hi={hi.round(2)} res={tuple(res)} vox={vox.round(4)}")

CACHE = f"/tmp/cloud_grid_{VOX}.npy"
import os
if os.path.exists(CACHE):
    grid = np.load(CACHE)
    print(f"loaded cached grid {CACHE}", flush=True)
else:
  grid = np.zeros(tuple(res), np.float32)
  # --- sparse splat: each Gaussian touches only its local 3-sigma voxel box ---
  for i in range(N):
    rad = 3.0 * s[i].max()
    i0 = np.clip(((mu[i] - rad - lo) / vox).astype(int), 0, res - 1)
    i1 = np.clip(((mu[i] + rad - lo) / vox).astype(int) + 1, 0, res)
    if np.any(i1 <= i0):
        continue
    gx = (np.arange(i0[0], i1[0]) + 0.5) * vox[0] + lo[0]
    gy = (np.arange(i0[1], i1[1]) + 0.5) * vox[1] + lo[1]
    gz = (np.arange(i0[2], i1[2]) + 0.5) * vox[2] + lo[2]
    P = np.stack(np.meshgrid(gx, gy, gz, indexing="ij"), -1) - mu[i]   # (a,b,c,3) world offset
    loc = P @ Rm[i] / s[i]                                             # R^T (x-mu) / s
    grid[i0[0]:i1[0], i0[1]:i1[1], i0[2]:i1[2]] += (amp[i] * np.exp(-0.5 * (loc ** 2).sum(-1))).astype(np.float32)
  np.save(CACHE, grid)
  print(f"cached grid -> {CACHE}", flush=True)

print(f"grid sigma_t (raw): max {grid.max():.2f} mean {grid.mean():.3f} nonzero {np.count_nonzero(grid)}", flush=True)
# Clamp the majorant for tractable delta-tracking. The dense cores are already fully opaque
# (tau >> 1 over a voxel), so clamping leaves the absorption IMAGE unchanged while slashing the
# majorant ~100x. (For a quantitative energy GT this clamp must be lifted + the grid refined.)
CLAMP = 80.0
grid = np.minimum(grid, CLAMP).astype(np.float32)
print(f"grid sigma_t (clamped@{CLAMP}): max {grid.max():.2f} mean {grid.mean():.3f}", flush=True)

# --- render with the cloud's exact cam_0000 (orthographic), matching our render ---
# cam_0000 from assets/models/cloud/__init__.py; extent 3.0 -> ortho scale.
SCALE = float(sys.argv[3]) if len(sys.argv) > 3 else 2.0
cam_to_world = (T().look_at(origin=[-3.98825, -0.306404, -1.74332e-07],
                            target=[-2.99119, -0.229803, -1.30749e-07],
                            up=[-0.076601, 0.997062, -3.34833e-09])
                @ T().scale([SCALE, SCALE, 1.0]))
scene = mi.load_dict({
    "type": "scene",
    "integrator": {"type": "volpath", "max_depth": 8},
    "object": {
        "type": "cube", "bsdf": {"type": "null"},
        "interior": {
            "type": "heterogeneous",
            "sigma_t": {"type": "gridvolume", "data": mi.TensorXf(grid),
                        "to_world": T().translate(lo.tolist()).scale(size.tolist())},
            "albedo": 0.0, "scale": 1.0,
        },
        "to_world": T().translate(((lo + hi) / 2).tolist()).scale((size / 2).tolist()),
    },
    "environment": {"type": "constant", "radiance": 1.0},
    "sensor": {
        "type": "orthographic", "to_world": cam_to_world,
        "film": {"type": "hdrfilm", "width": 900, "height": 600,
                 "pixel_format": "rgb", "rfilter": {"type": "box"}},
    },
})
print(f"rendering {SPP} spp...", flush=True)
img = mi.render(scene, spp=SPP)
a = np.array(img)
print(f"render mean {a.mean():.4f}", flush=True)
mi.util.write_bitmap("results/campaign/voxel_cloud.exr", img)
mi.util.write_bitmap("results/campaign/voxel_cloud.png", img)
print("wrote results/campaign/voxel_cloud.{exr,png}")
