#!/usr/bin/env python3
"""Scattering voxel-GT via AdVol (Jorge's DSYG grid baseline): render our cloud density grid
with LOCAL supervoxel majorants + residual ratio tracking (ff_local / rrt_local) -- the
variance-reduction machinery stock Mitsuba lacks. UNCLAMPED (local majorants handle the high
dynamic range). Renders K seeds from the exact cloud scene (cam_0000) under the meadow envmap,
albedo 0.9, HG g=0.85, and reports inter-seed variance (k) + firefly stats vs the prior
stock-Mitsuba attempt (which was firefly-limited, RMSE ~1.13).

Usage: tools/refs/.venv/bin/python -u tools/refs/voxel_gt_advol.py <grid.npz> [spp] [nseeds] [maxdepth]
"""
import sys, os
import numpy as np
import mitsuba as mi
mi.set_variant("cuda_ad_rgb")
import advol
advol.register()
T = mi.ScalarTransform4f

GRID = sys.argv[1]
SPP = int(sys.argv[2]) if len(sys.argv) > 2 else 64
NSEEDS = int(sys.argv[3]) if len(sys.argv) > 3 else 4
MAXD = int(sys.argv[4]) if len(sys.argv) > 4 else 128
ENV = "/home/kacper/thesis/assets/environment_maps/meadow_2_4k.hdr"

d = np.load(GRID)
grid, lo, hi = d["grid"], d["lo"], d["hi"]
grid_m = np.ascontiguousarray(grid.transpose(2, 1, 0))   # our [X,Y,Z] -> [Z,Y,X]
print(f"grid {grid.shape} peak {grid.max():.1f} mean {grid.mean():.3f} UNCLAMPED spp={SPP} seeds={NSEEDS} maxd={MAXD}", flush=True)

vd = advol.VolumeData.from_array(grid_m, bbox_min=tuple(lo.tolist()), bbox_max=tuple(hi.tolist()))
medium = advol.build_medium(
    sigma_t=vd, albedo=0.9, phase={"type": "hg", "g": 0.85},
    majorant_factor=1.01, supervoxel_factor=4, medium_id="cloud",
)

sys.path.insert(0, "assets/models/cloud")
import __init__ as cloud  # noqa
scene_dict = {"type": "scene"}
scene_dict.update({k: vv for k, vv in cloud.OBJECTS.items() if k not in ("resources", "primitives_pyr0")})
scene_dict.update({k: vv for k, vv in cloud.SENSORS.items() if k == "cam_0000"})
scene_dict["environment"] = {"type": "envmap", "filename": ENV, "to_world": T().rotate(axis=[0, 1, 0], angle=90.0)}
scene_dict["integrator"] = {
    "type": "advol", "distance_sampler": "ff_local",
    "transmittance_estimator": "rrt_local", "max_depth": MAXD, "supervoxel_factor": 4,
}
size = hi - lo
scene_dict["cloud_grid"] = {
    "type": "cube", "bsdf": {"type": "null"},
    "to_world": T().translate(((lo + hi) / 2).tolist()).scale((size / 2).tolist()),
    "interior": medium,
}
scene = mi.load_dict(scene_dict)

tag = os.path.splitext(os.path.basename(GRID))[0]
outdir = "results/campaign/advol_seeds"; os.makedirs(outdir, exist_ok=True)
imgs = []
for s in range(NSEEDS):
    raw = mi.render(scene, spp=SPP, seed=s)
    if s == 0:
        mi.util.write_bitmap(f"{outdir}/advol_{tag}_seed0.exr", raw)
        mi.util.write_bitmap(f"{outdir}/advol_{tag}_seed0.png", raw)
    img = np.array(raw)[..., :3]
    imgs.append(img)
    print(f"  seed {s}: mean={img.mean():.4f} max={img.max():.1f}", flush=True)
A = np.stack(imgs)
kraw = float(A.var(0, ddof=1).mean() * SPP)
P = np.percentile(A, 99.9)
kclip = float(np.minimum(A, P).var(0, ddof=1).mean() * SPP)
mean = float(A.mean()); mx = float(A.max())
ff = int((A.max(-1) > 20 * mean).sum() / NSEEDS)
print("---")
print(f"AdVol scattering GT ({tag}, unclamped, ff_local/rrt_local): mean={mean:.4f} max={mx:.1f}")
print(f"  k_raw={kraw:.3f}  k_clip999={kclip:.3f}  fireflies(>20x mean)/img={ff}")
print(f"  vs ours-meadow mean 0.3214 -> ratio {mean/0.3214:.3f}")
print(f"  vs prior STOCK attempt: clamp400 RMSE 1.13 (firefly-limited), analog k~3899")
