#!/usr/bin/env python3
"""Voxelize the cloud's Gaussians into a dense sigma_t grid in OUR renderer's convention, and cache it.
Build-only (no render). Optional supersampling (SS): build at SS*VOX then box-average down to VOX, so
each voxel is the SS^3-sample average of the field (true voxel average as SS->inf) -> faster, more
honest convergence than point-sampling.

Convention (matches device/core/primitive.h):
  sigma_t(x) = sum_i (SIGMA_MULT * sigma_t_i) * (2*pi)^(-3/2) * prod(1/s_ij) * exp(-1/2 ||S^-1 R^T (x-mu)||^2)

Usage: experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/voxel_build.py VOX [SS]
Output: results/campaign/voxgrids/cloud_<VOX>_ss<SS>.npz  (grid, lo, hi, peak)
"""
import sys, os, time
import numpy as np
from plyfile import PlyData
from scipy.spatial.transform import Rotation

SIGMA_MULT = 7.5
NORM = (2 * np.pi) ** (-1.5)
PLY = "assets/models/cloud/root.primitives_pyr0.ply"

VOX = int(sys.argv[1])
SS = int(sys.argv[2]) if len(sys.argv) > 2 else 1
BUILD = VOX * SS                                   # internal (supersampled) resolution

v = PlyData.read(PLY)["vertex"]
mu = np.stack([v["x"], v["y"], v["z"]], -1).astype(np.float64)
s = np.exp(np.stack([v["scale_0"], v["scale_1"], v["scale_2"]], -1)).astype(np.float64)
quat = np.stack([v["rot_0"], v["rot_1"], v["rot_2"], v["rot_3"]], -1).astype(np.float64)  # xyzw
sigt = np.array(v["sigma_t_0"]).astype(np.float64)
Rm = Rotation.from_quat(quat).as_matrix()
amp = SIGMA_MULT * sigt * NORM / np.prod(s, axis=1)
N = len(mu)

lo = (mu - 3 * s.max(1, keepdims=True)).min(0)
hi = (mu + 3 * s.max(1, keepdims=True)).max(0)
size = hi - lo
res = np.maximum((BUILD * size / size.max()).round().astype(int), 8)
vox = size / res
grid = np.zeros(tuple(res), np.float32)
print(f"N={N} VOX={VOX} SS={SS} build_res={tuple(res)} vox={vox.round(4)}", flush=True)

t0 = time.time()
for i in range(N):
    rad = 3.0 * s[i].max()
    i0 = np.clip(((mu[i] - rad - lo) / vox).astype(int), 0, res - 1)
    i1 = np.clip(((mu[i] + rad - lo) / vox).astype(int) + 1, 0, res)
    if np.any(i1 <= i0):
        continue
    gx = (np.arange(i0[0], i1[0]) + 0.5) * vox[0] + lo[0]
    gy = (np.arange(i0[1], i1[1]) + 0.5) * vox[1] + lo[1]
    gz = (np.arange(i0[2], i1[2]) + 0.5) * vox[2] + lo[2]
    P = np.stack(np.meshgrid(gx, gy, gz, indexing="ij"), -1) - mu[i]
    loc = P @ Rm[i] / s[i]
    grid[i0[0]:i1[0], i0[1]:i1[1], i0[2]:i1[2]] += (amp[i] * np.exp(-0.5 * (loc ** 2).sum(-1))).astype(np.float32)
    if i % 100 == 0:
        print(f"  splat {i}/{N} ({time.time()-t0:.0f}s)", flush=True)

# box-average down by SS (true voxel average)
if SS > 1:
    nz = (res // SS) * SS
    grid = grid[:nz[0], :nz[1], :nz[2]].reshape(nz[0]//SS, SS, nz[1]//SS, SS, nz[2]//SS, SS).mean((1, 3, 5)).astype(np.float32)

out = f"results/campaign/voxgrids/cloud_{VOX}_ss{SS}.npz"
np.savez(out, grid=grid, lo=lo, hi=hi, peak=float(grid.max()))
print(f"final {grid.shape}  peak {grid.max():.1f} mean {grid.mean():.3f}  -> {out}  ({time.time()-t0:.0f}s)", flush=True)
