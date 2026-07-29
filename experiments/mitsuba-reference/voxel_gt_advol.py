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

# --- halo-aware majorant fix (default on) --------------------------------------
# AdVol's supervoxel majorant is a STRICT per-block max (no halo). Trilinear
# interpolation near a block boundary reads voxels from the neighbouring block,
# which can exceed this block's max -> majorant violation -> biased delta tracking
# -> visible block artifacts. Dilating the field by one voxel (3^3 max) BEFORE the
# block-max makes each block's majorant bound the interpolated field that reaches
# into its neighbours. Only the "max" reduction (majorant) is dilated; "mean"
# (the RRT control coefficient) is left untouched (it affects variance, not bias).
if os.environ.get("ADVOL_HALO", "1") != "0":
    import advol.supervoxel as _sv
    _orig_block_reduce = _sv._block_reduce
    def _halo_block_reduce(data, factor, op):
        if op == "max" and factor > 1:
            d = data
            for ax in (0, 1, 2):
                d = np.maximum(d, np.roll(d, 1, axis=ax))
                d = np.maximum(d, np.roll(d, -1, axis=ax))
            data = d
        return _orig_block_reduce(data, factor, op)
    _sv._block_reduce = _halo_block_reduce
    print("[halo majorant fix ACTIVE]", flush=True)

GRID = sys.argv[1]
SPP = int(sys.argv[2]) if len(sys.argv) > 2 else 64
NSEEDS = int(sys.argv[3]) if len(sys.argv) > 3 else 4
MAXD = int(sys.argv[4]) if len(sys.argv) > 4 else 128
# config knobs (env): distance sampler / transmittance estimator / majorant headroom / supervoxel block
DS = os.environ.get("ADVOL_DS", "ff_local")
TE = os.environ.get("ADVOL_TE", "rrt_local")
MF = float(os.environ.get("ADVOL_MF", "1.01"))
SVF = int(os.environ.get("ADVOL_SVF", "4"))
CLAMP = float(os.environ.get("ADVOL_CLAMP", "0"))   # 0 = unclamped; else cap sigma_t (lowers global majorant)
TAGSUF = os.environ.get("ADVOL_TAG", "")
ENV = "/home/kacper/thesis/assets/environment_maps/meadow_2_4k.hdr"

d = np.load(GRID)
grid, lo, hi = d["grid"], d["lo"], d["hi"]
raw_peak = float(grid.max())
if CLAMP > 0:
    grid = np.minimum(grid, CLAMP).astype(np.float32)
grid_m = np.ascontiguousarray(grid.transpose(2, 1, 0))   # our [X,Y,Z] -> [Z,Y,X]
print(f"grid {grid.shape} raw_peak {raw_peak:.1f} CLAMP {CLAMP} -> peak {grid.max():.1f} mean {grid.mean():.3f} "
      f"spp={SPP} seeds={NSEEDS} maxd={MAXD}", flush=True)

vd = advol.VolumeData.from_array(grid_m, bbox_min=tuple(lo.tolist()), bbox_max=tuple(hi.tolist()))
print(f"config: DS={DS} TE={TE} majorant_factor={MF} supervoxel_factor={SVF}", flush=True)
medium = advol.build_medium(
    sigma_t=vd, albedo=0.9, phase={"type": "hg", "g": 0.85},
    majorant_factor=MF, supervoxel_factor=SVF, medium_id="cloud",
)

sys.path.insert(0, "assets/models/cloud")
import __init__ as cloud  # noqa
scene_dict = {"type": "scene"}
scene_dict.update({k: vv for k, vv in cloud.OBJECTS.items() if k not in ("resources", "primitives_pyr0")})
scene_dict.update({k: vv for k, vv in cloud.SENSORS.items() if k == "cam_0000"})
scene_dict["environment"] = {"type": "envmap", "filename": ENV, "to_world": T().rotate(axis=[0, 1, 0], angle=90.0)}
scene_dict["integrator"] = {
    "type": "advol", "distance_sampler": DS,
    "transmittance_estimator": TE, "max_depth": MAXD, "supervoxel_factor": SVF,
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
tag = tag + TAGSUF
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
if NSEEDS > 1:
    import mitsuba as _mi
    mean_img = A.mean(0).astype(np.float32)
    _mi.util.write_bitmap(f"{outdir}/advol_{tag}_mean.exr", _mi.TensorXf(mean_img))
    _mi.util.write_bitmap(f"{outdir}/advol_{tag}_mean.png", _mi.TensorXf(mean_img))
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
