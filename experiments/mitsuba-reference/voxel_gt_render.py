#!/usr/bin/env python3
"""Render a cached cloud density grid through Mitsuba's INDEPENDENT heterogeneous/gridvolume path
tracer, from the EXACT cloud scene (cam_0000), UNCLAMPED, with a majorant supergrid for tractability.
Reports RMSE vs our renderer's reference. This is the valid-GT render (no density clamp).

Usage: tools/refs/with_jorge_mitsuba.sh tools/refs/.venv/bin/python -u tools/refs/voxel_gt_render.py \
          <grid.npz> <absorption|scattering> [spp] [majorant_factor]
"""
import sys, os
import numpy as np
import OpenEXR, Imath
import mitsuba as mi
mi.set_variant("cuda_ad_rgb")
import volprim.integrators.volprim_prb  # noqa
T = mi.ScalarTransform4f

GRID = sys.argv[1]
MODE = sys.argv[2] if len(sys.argv) > 2 else "absorption"
SPP = int(sys.argv[3]) if len(sys.argv) > 3 else 64
CLAMP = float(sys.argv[4]) if len(sys.argv) > 4 else 0.0   # 0 = unclamped; else cap sigma_t (lowers majorant)
ENV = "/home/kacper/thesis/assets/environment_maps/meadow_2_4k.hdr"

d = np.load(GRID)
grid, lo, hi = d["grid"], d["lo"], d["hi"]
size = hi - lo
raw_peak = float(grid.max())
if CLAMP > 0:
    grid = np.minimum(grid, CLAMP).astype(np.float32)
grid_m = np.ascontiguousarray(grid.transpose(2, 1, 0))    # our [X,Y,Z] -> Mitsuba [Z,Y,X]
print(f"grid {grid.shape} raw_peak {raw_peak:.1f} clamp {CLAMP} -> peak {grid.max():.1f} mean {grid.mean():.3f}  mode={MODE} spp={SPP}", flush=True)

sys.path.insert(0, "assets/models/cloud")
import __init__ as cloud  # noqa

scene_dict = {"type": "scene"}
scene_dict.update({k: vv for k, vv in cloud.OBJECTS.items() if k not in ("resources", "primitives_pyr0")})
scene_dict.update({k: vv for k, vv in cloud.EMITTERS.items() if k != "resources"})  # constant env
scene_dict.update({k: vv for k, vv in cloud.SENSORS.items() if k == "cam_0000"})
scene_dict["integrator"] = {"type": "prbvolpath", "max_depth": 128}

medium = {
    "type": "heterogeneous",
    "sigma_t": {"type": "gridvolume", "data": mi.TensorXf(grid_m),
                "to_world": T().translate(lo.tolist()).scale(size.tolist())},
    "scale": 1.0,
}
if MODE == "scattering":
    medium["albedo"] = {"type": "uniform", "value": 0.9}
    medium["phase"] = {"type": "hg", "g": 0.85}
    scene_dict["environment"] = {"type": "envmap", "filename": ENV,
                                 "to_world": T().rotate(axis=[0, 1, 0], angle=90.0)}
else:  # absorption: albedo 0, keep the scene's constant white env
    medium["albedo"] = 0.0

scene_dict["cloud_grid"] = {
    "type": "cube", "bsdf": {"type": "null"},
    "to_world": T().translate(((lo + hi) / 2).tolist()).scale((size / 2).tolist()),
    "interior": medium,
}

scene = mi.load_dict(scene_dict)
print("rendering ...", flush=True)
img = mi.render(scene, spp=SPP)
arr = np.array(img)

tag = os.path.splitext(os.path.basename(GRID))[0]
outbase = f"results/campaign/voxgt_{MODE}_{tag}_c{int(CLAMP)}"
mi.util.write_bitmap(outbase + ".exr", img)
mi.util.write_bitmap(outbase + ".png", img)

# RMSE vs our reference
def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()["dataWindow"]
    w = dw.max.x - dw.min.x + 1; h = dw.max.y - dw.min.y + 1; pt = Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c, pt), np.float32).reshape(h, w) for c in ("R", "G", "B")], -1)

ref_path = ("results/campaign/ico_fig/analytic.exr" if MODE == "absorption" else None)
if MODE == "scattering":
    import glob
    fs = sorted(glob.glob("results/campaign/g1_seeds/cuda_seed*.exr"))
    ref = np.mean([load(f) for f in fs], 0) if fs else None
else:
    ref = load(ref_path) if os.path.exists(ref_path) else None

if ref is not None and ref.shape == arr.shape:
    rmse = float(np.sqrt(np.mean((arr - ref) ** 2)))
    print(f"render mean {arr.mean():.4f}  ref mean {ref.mean():.4f}  RMSE {rmse:.4f}", flush=True)
else:
    print(f"render mean {arr.mean():.4f}  (no matched ref for RMSE)", flush=True)
print(f"wrote {outbase}.{{exr,png}}", flush=True)
