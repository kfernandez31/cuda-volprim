"""Convert the wdas4_12_1 same-cloud fit family (multiple budgets, same Disney cloud)
from npy_data arrays into our renderer's PLY format, for the R3 same-object scaling study.

Reuses the validated transform of npy_asset_to_ply.py exactly: scales LINEAR -> log
(loader applies expf), quaternions Mitsuba xyzw -> our (w,x,y,z), opacities -> sigma_t,
property order x,y,z,rot_0..3,scale_0..2,albedo_0..2,sigma_t_0 (albedo=0, overridden at render).

Writes assets/models/wdas_scaling/wdas_<N>.ply for each fit.
"""
import os, numpy as np

BASE = "assets/models/unpacked/"
OUT = "assets/models/wdas_scaling/"
os.makedirs(OUT, exist_ok=True)

# (N, fit-dir, pyr-level): all the SAME Disney cloud (wdas4_12_1) at different fit budgets
FITS = [
    (2048,  "wdas4_12_1_small/optimized_asset_pyr0", "pyr0"),
    (4096,  "wdas4_12_1/optimized_asset_pyr0",       "pyr0"),
    (8192,  "wdas4_12_1_small/optimized_asset_pyr1", "pyr1"),
    (12288, "wdas4_12_1_mid/optimized_asset_pyr1",   "pyr1"),
    (32768, "wdas4_12_1/optimized_asset_pyr1",       "pyr1"),
    (65536, "wdas4_12_1_big/optimized_asset_pyr1",   "pyr1"),
]
PROPS = ["x", "y", "z", "rot_0", "rot_1", "rot_2", "rot_3",
         "scale_0", "scale_1", "scale_2", "albedo_0", "albedo_1", "albedo_2", "sigma_t_0"]


def convert(N, fit_dir, pyr):
    nd = os.path.join(BASE, fit_dir, "npy_data")
    centers = np.load(f"{nd}/centers_{pyr}.npy").astype(np.float64).reshape(-1, 3)
    scales = np.load(f"{nd}/scales_{pyr}.npy").astype(np.float64).reshape(-1, 3)
    quats = np.load(f"{nd}/quaternions_{pyr}.npy").astype(np.float64).reshape(-1, 4)
    opac = np.load(f"{nd}/opacities_{pyr}.npy").astype(np.float64).reshape(-1)
    n = len(centers)
    assert n == N, f"{fit_dir}: expected {N} got {n}"
    w, x, y, z = quats[:, 3], quats[:, 0], quats[:, 1], quats[:, 2]   # xyzw -> w,x,y,z
    log_scale = np.log(np.clip(scales, 1e-12, None))
    rows = np.zeros((n, len(PROPS)), dtype=np.float32)
    rows[:, 0:3] = centers
    rows[:, 3] = w; rows[:, 4] = x; rows[:, 5] = y; rows[:, 6] = z
    rows[:, 7:10] = log_scale
    rows[:, 10:13] = 0.0
    rows[:, 13] = opac
    out = f"{OUT}wdas_{N}.ply"
    with open(out, "wb") as f:
        hdr = "ply\nformat binary_little_endian 1.0\n" + f"element vertex {n}\n"
        hdr += "".join(f"property float {p}\n" for p in PROPS) + "end_header\n"
        f.write(hdr.encode("ascii"))
        f.write(rows.tobytes())
    print(f"wrote {out}  (N={n}, opac {opac.min():.4f}..{opac.max():.4f} sum {opac.sum():.1f})")


for N, d, pyr in FITS:
    convert(N, d, pyr)
