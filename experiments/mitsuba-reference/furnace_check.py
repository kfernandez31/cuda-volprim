"""Furnace energy-conservation gate (reference-free).

A conservative medium (albedo=1) in a CONSTANT radiance field must render perfectly flat:
L = L_env everywhere (the Gaussian is invisible). This holds for ANY phase function (HG g≠0)
or MIS setting — dL/ds = −σ_t·L + σ_t·L_env = 0 when albedo=1 and L=L_env. So it is the
energy gate for every new toggle (WS2 HG, WS3 MIS). No reference render needed.

Pass criterion: mean ≈ 1.0 and the residual is pure zero-mean MC noise (no systematic
darkening/brightening, i.e. |mean−1| within a few × SEM, and no large-scale structure).

Usage: furnace_check.py <rendered.exr> [expected_env_value=1.0]
Exit code 0 = PASS, 1 = FAIL.
"""
import sys
import numpy as np
import OpenEXR, Imath


def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()["dataWindow"]
    sz = (dw.max.y - dw.min.y + 1, dw.max.x - dw.min.x + 1)
    ch = f.channels(["R", "G", "B"], Imath.PixelType(Imath.PixelType.FLOAT))
    return np.stack([np.frombuffer(c, np.float32).reshape(sz) for c in ch], -1)


img = load(sys.argv[1])
env = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0
d = img - env
mean = img.mean()
sem = img.std() / np.sqrt(img.size)
# large-scale structure: blur and check for any sustained bias region
from scipy.ndimage import uniform_filter
blur = uniform_filter(d.mean(-1), size=33)
worst_region = np.abs(blur).max()

bias = mean - env
# Tolerances: |bias| within 5×SEM (statistical flatness) AND blurred structure < 2e-3.
pass_bias = abs(bias) < max(5 * sem, 2e-3)
pass_struct = worst_region < 3e-3
ok = pass_bias and pass_struct

print(f"furnace {sys.argv[1].split('/')[-1]}")
print(f"  mean={mean:.5f} (env={env})  bias={bias:+.5f}  SEM={sem:.5f}  ({abs(bias)/sem:.1f}σ)")
print(f"  per-channel mean: R={img[...,0].mean():.5f} G={img[...,1].mean():.5f} B={img[...,2].mean():.5f}")
print(f"  blurred max|bias region|={worst_region:.5f}   min={img.min():.4f} max={img.max():.4f}")
print(f"  bias {'OK' if pass_bias else 'FAIL'} | structure {'OK' if pass_struct else 'FAIL'}  =>  "
      f"{'PASS' if ok else 'FAIL'}")
sys.exit(0 if ok else 1)
