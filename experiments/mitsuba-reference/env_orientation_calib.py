"""WS0 env-orientation calibration.

Compare the CUDA perspective+meadow background against each Mitsuba envmap-rotation
candidate, in both vertical orientations (CUDA saves flip_vertical=True; Mitsuba's
hdrfilm may differ). Reports per-candidate RGB RMSE so we can pick the to_world that
makes the two backgrounds match pixel-for-pixel.

Usage:
    experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/env_orientation_calib.py CUDA.exr MITS1.exr [MITS2.exr ...]
"""
import sys
import numpy as np
import OpenEXR
import Imath


def load_rgb(p):
    f = OpenEXR.InputFile(p)
    dw = f.header()["dataWindow"]
    sz = (dw.max.y - dw.min.y + 1, dw.max.x - dw.min.x + 1)
    ch = f.channels(["R", "G", "B"], Imath.PixelType(Imath.PixelType.FLOAT))
    return np.stack([np.frombuffer(c, np.float32).reshape(sz) for c in ch], -1)


def rmse(a, b):
    return float(np.sqrt(np.mean((a - b) ** 2)))


cuda = load_rgb(sys.argv[1])
print(f"CUDA ref: {sys.argv[1]}")
print(f"  shape={cuda.shape} mean={cuda.mean():.4f} max={cuda.max():.4f}\n")

results = []
for p in sys.argv[2:]:
    m = load_rgb(p)
    if m.shape != cuda.shape:
        print(f"  SKIP shape mismatch {p}: {m.shape}")
        continue
    r_id = rmse(cuda, m)
    r_fl = rmse(cuda, np.flipud(m))
    best = min(r_id, r_fl)
    orient = "as-is" if r_id <= r_fl else "flipud"
    results.append((best, p, orient, r_id, r_fl))

results.sort(key=lambda t: t[0])
print(f"{'RMSE':>9}  {'orient':>7}  {'rmse_asis':>9} {'rmse_flip':>9}  file")
for best, p, orient, r_id, r_fl in results:
    name = p.split("/")[-1]
    print(f"{best:9.5f}  {orient:>7}  {r_id:9.5f} {r_fl:9.5f}  {name}")

if results:
    best = results[0]
    print(f"\nBEST: {best[1].split('/')[-1]}  orient={best[2]}  RMSE={best[0]:.5f}")
