#!/usr/bin/env python3
"""Reduced-density cloud comparison: ours | Mitsuba-analog | signed rel diff.

The production-density cloud is optically deep (core T ~ 0), so a transmittance
image compresses interior errors into black. This panel repeats the absorption
comparison with every sigma_t scaled to 4% of production (multiplier 0.3 vs 7.5),
which puts the whole volume in the visible range (min T ~ 0.17): every interior
error would be directly visible. Display is a plain gamma tonemap -- deliberately
NO contrast stretch, so the panel proves nothing clips to black.

Reference arm: volprim_prb in analog mode (use_nee=False), mean of N seeds at
4096 spp each. Analog because the released NEE estimator deterministically
scales all direct environment radiance by 1/(1+(4pi)^-2) ~ 0.99371 (depth-0 MIS
weight against an unmasked emitter pdf; see results/campaign/reduced_density/).

Inputs:  results/campaign/reduced_density/thin_ours.exr
         results/campaign/reduced_density/ref_analog_seed*.exr
Output:  latex/figures/reduced_density.pdf (+ .png)
"""
import glob
import os

import numpy as np
import OpenEXR, Imath
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

SRC = "results/campaign/reduced_density"
OUT = "latex/figures/reduced_density.pdf"
DIFF_SCALE = 0.05  # +-5% full scale, thesis-wide comparison-plot convention


def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()["dataWindow"]
    w = dw.max.x - dw.min.x + 1; h = dw.max.y - dw.min.y + 1
    pt = Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c, pt), np.float32).reshape(h, w)
                     for c in ("R", "G", "B")], -1)


ours = load(os.path.join(SRC, "thin_ours.exr"))
seed_files = sorted(glob.glob(os.path.join(SRC, "ref_analog_seed*.exr")))
assert seed_files, "no reference seeds banked yet"
seeds = [load(f) for f in seed_files]
ref = np.mean(seeds, 0)

# Metrics on raw values (display is tonemapped, numbers are not).
rmse = float(np.sqrt(((ours - ref) ** 2).mean()))
ratio = float(ours.mean() / ref.mean())
# Reference-arm noise floor: per-pixel std of the seed mean.
noise = float(np.sqrt(np.mean(np.var(np.stack(seeds), 0) / len(seeds))))
# Optical-depth agreement over medium pixels (both arms clip-protected).
to = -np.log(ours.mean(-1).clip(1e-6, 1.0)); tr = -np.log(ref.mean(-1).clip(1e-6, 1.0))
m = tr > 0.01
tau_ratio = float(to[m].mean() / tr[m].mean())

diff_rel = (ours.mean(-1) - ref.mean(-1)) / np.maximum(ref.mean(-1), 1e-3)
t = np.clip(diff_rel / DIFF_SCALE, -1, 1)
pos, neg = np.clip(t, 0, 1), np.clip(-t, 0, 1)
diff = np.stack([1 - 0.85 * neg, 1 - 0.85 * pos - 0.55 * neg, 1 - 0.85 * pos], -1)

gamma = lambda a: np.clip(a, 0, 1) ** (1 / 2.2)

fig, axes = plt.subplots(1, 3, figsize=(8.4, 2.1))
for ax, img, title in zip(axes, [gamma(ours), gamma(ref), diff],
                          ["ours", "reference (analog)",
                           f"signed rel. diff ($\\pm${DIFF_SCALE*100:.0f}%)"]):
    ax.imshow(img); ax.axis("off"); ax.set_title(title, fontsize=10)
axes[2].text(0.5, -0.06, f"ratio {ratio:.4f},  RMSE {rmse:.4f}",
             transform=axes[2].transAxes, ha="center", va="top", fontsize=10)
plt.subplots_adjust(wspace=0.04, left=0.01, right=0.99, top=0.88, bottom=0.06)
fig.savefig(OUT, dpi=150, bbox_inches="tight")
fig.savefig(os.path.splitext(OUT)[0] + ".png", dpi=150, bbox_inches="tight")
print(f"seeds={len(seeds)}  ratio={ratio:.5f}  RMSE={rmse:.5f}  "
      f"noise_floor={noise:.5f}  tau_ratio={tau_ratio:.5f}  "
      f"minT ours={ours.min():.4f} ref={ref.min():.4f}")
print("wrote", OUT)
