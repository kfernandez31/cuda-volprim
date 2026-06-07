"""Bit-exact / max-abs EXR diff for A/B validation. Usage: exr_diff.py a.exr b.exr"""
import sys
import numpy as np
import OpenEXR, Imath


def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()["dataWindow"]
    sz = (dw.max.y - dw.min.y + 1, dw.max.x - dw.min.x + 1)
    ch = f.channels(["R", "G", "B"], Imath.PixelType(Imath.PixelType.FLOAT))
    return np.stack([np.frombuffer(c, np.float32).reshape(sz) for c in ch], -1)


a, b = load(sys.argv[1]), load(sys.argv[2])
d = np.abs(a - b)
n_diff = int((d > 0).sum())
print(f"shape {a.shape}  max|Δ| {d.max():.3e}  mean|Δ| {d.mean():.3e}  "
      f"ndiff {n_diff}/{a.size}  meanA {a.mean():.6f}  meanB {b.mean():.6f}  "
      f"signed-mean Δ {(a-b).mean():+.3e}")
print("BIT-IDENTICAL" if n_diff == 0 else "NOT bit-identical")
