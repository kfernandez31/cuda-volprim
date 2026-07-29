"""Generate the three synthetic scaling-v2 families for §7.6 (redesigned).

Each family holds total optical depth (roughly) constant while N grows, and pins
the number of primitives a camera ray crosses BY CONSTRUCTION, giving a derived
scaling exponent to test against:

  sheet_n<k>  : k x k lattice in the z=0 plane, fixed footprint [-L,L]^2,
                sigma = 0.3 d (d = lattice spacing), sigma_t = TAU * d^2.
                A -z camera ray crosses O(1) supports at every N  -> slope ~ 0.
  cube_n<k>   : k^3 lattice over [-L,L]^3, sigma = 0.3 d, sigma_t = TAU * d^2 / k.
                A -z ray crosses ~k layers                          -> slope ~ 1/3.
  stack_N<M>  : M frame-covering pancakes along z in [-L,L],
                sigma_xy = 1.0, sigma_z = 0.25 dz, sigma_t = TAU * 2*pi / M.
                Every ray crosses ALL M supports                    -> slope ~ 1.

PLY format identical to the asset converters in experiments/mitsuba-reference:
binary_little_endian, property order x,y,z, rot_0..3 (w,x,y,z), scale_0..2
(LOG scales; loader expf's), albedo_0..2 (0 -> absorption), sigma_t_0.
Identity rotation throughout.

Camera for all families: asset_validation defaults (perspective, dist 3.5, fov 40,
SG_VIEW=negz), which frames [-1,1]^2 with margin; L=0.9 keeps everything inside.

Usage: python3 scripts/tools/gen_scaling_v2.py [--tau 1.0]
Writes assets/synthetic/scaling_v2/*.ply + manifest.csv.
"""
import argparse
import math
import os

import numpy as np

OUT = "assets/synthetic/scaling_v2/"
PROPS = ["x", "y", "z", "rot_0", "rot_1", "rot_2", "rot_3",
         "scale_0", "scale_1", "scale_2", "albedo_0", "albedo_1", "albedo_2",
         "sigma_t_0"]
L = 0.9

SHEET_K = [4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 56, 64, 72, 80, 91]
CUBE_K = [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 22, 24]
STACK_M = [8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 448, 512]


def write_ply(path, centers, sigmas_xyz, sigma_t):
    n = len(centers)
    rows = np.zeros((n, len(PROPS)), dtype=np.float32)
    rows[:, 0:3] = centers
    rows[:, 3] = 1.0  # identity quaternion (w,x,y,z)
    rows[:, 7:10] = np.log(np.asarray(sigmas_xyz, dtype=np.float64))
    rows[:, 13] = sigma_t
    with open(path, "wb") as f:
        hdr = "ply\nformat binary_little_endian 1.0\n" + f"element vertex {n}\n"
        hdr += "".join(f"property float {p}\n" for p in PROPS) + "end_header\n"
        f.write(hdr.encode("ascii"))
        f.write(rows.tobytes())
    return n


def lattice(k, dims):
    """k points per axis over [-L, L] in each of `dims` axes, cell-centred."""
    d = 2.0 * L / k
    ax = -L + d * (np.arange(k) + 0.5)
    if dims == 2:
        gx, gy = np.meshgrid(ax, ax, indexing="ij")
        pts = np.stack([gx.ravel(), gy.ravel(), np.zeros(k * k)], axis=1)
    else:
        gx, gy, gz = np.meshgrid(ax, ax, ax, indexing="ij")
        pts = np.stack([gx.ravel(), gy.ravel(), gz.ravel()], axis=1)
    return pts, d


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tau", type=float, default=1.0,
                    help="target central optical depth per family")
    a = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    rows = ["family,label,k,N,spacing,sigma,sigma_t,pred_hits_per_ray"]

    for k in SHEET_K:
        pts, d = lattice(k, 2)
        sig = 0.3 * d
        st = a.tau * d * d
        n = write_ply(f"{OUT}sheet_n{k}.ply", pts, [sig, sig, sig], st)
        rows.append(f"sheet,sheet_n{k},{k},{n},{d:.6f},{sig:.6f},{st:.8f},O(1)")

    for k in CUBE_K:
        pts, d = lattice(k, 3)
        sig = 0.3 * d
        st = a.tau * d * d / k
        n = write_ply(f"{OUT}cube_n{k}.ply", pts, [sig, sig, sig], st)
        rows.append(f"cube,cube_n{k},{k},{n},{d:.6f},{sig:.6f},{st:.8f},~{k}")

    for m in STACK_M:
        dz = 2.0 * L / m
        z = -L + dz * (np.arange(m) + 0.5)
        pts = np.stack([np.zeros(m), np.zeros(m), z], axis=1)
        sig_xy, sig_z = 1.0, 0.25 * dz
        st = a.tau * 2.0 * math.pi / m
        n = write_ply(f"{OUT}stack_N{m}.ply", pts, [sig_xy, sig_xy, sig_z], st)
        rows.append(f"stack,stack_N{m},{m},{n},{dz:.6f},{sig_z:.6f},{st:.8f},{m}")

    with open(f"{OUT}manifest.csv", "w") as f:
        f.write("\n".join(rows) + "\n")
    print(f"wrote {len(rows) - 1} configs to {OUT} (tau={a.tau})")


if __name__ == "__main__":
    main()
