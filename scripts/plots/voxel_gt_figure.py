#!/usr/bin/env python3
"""Assemble fig:voxel-gt (thesis Sec 5.2.1) from the banked absorption cross-check artifacts.

Panels: analytic Gaussian renderer | 600^3 dense-grid delta-tracking render (Mitsuba
heterogeneous/gridvolume, 32 spp, single seed) | |difference| x5.
Inputs are the banked EXRs of the voxel-gt campaign (results/campaign/voxel_gt.md):
  results/campaign/ico_fig/analytic.exr                       (analytic arm; mean 0.4163)
  results/campaign/voxgt_absorption_cloud_600_ss1_c250.exr    (600^3 grid arm; mean 0.4011)
Run under experiments/mitsuba-reference/.venv (needs mitsuba + matplotlib). Output: latex/figures/voxel_gt.pdf
"""
import numpy as np
import mitsuba as mi

mi.set_variant("scalar_rgb")
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def load(path):
    return np.array(mi.Bitmap(path)).astype(np.float64)[..., :3]


ours = load("results/campaign/ico_fig/analytic.exr")
grid = load("results/campaign/voxgt_absorption_cloud_600_ss1_c250.exr")
diff = np.abs(ours - grid)
print(f"analytic mean {ours.mean():.4f}  grid mean {grid.mean():.4f}  "
      f"median |diff| {np.median(diff):.4g}")

tone = lambda a: np.clip(a, 0, 1) ** (1 / 2.2)
panels = [
    (tone(ours), "analytic (this renderer)"),
    (tone(grid), r"$600^3$ grid, delta tracking"),
    (tone(diff * 5.0), r"$|\Delta| \times 5$"),
]
fig, axes = plt.subplots(1, 3, figsize=(10.5, 3.0))
for ax, (img, title) in zip(axes, panels):
    ax.imshow(img)
    ax.set_title(title, fontsize=10)
    ax.set_xticks([]); ax.set_yticks([])
fig.tight_layout()
for ext in ("pdf", "png"):
    fig.savefig(f"latex/figures/voxel_gt.{ext}", dpi=130, bbox_inches="tight")
print("wrote latex/figures/voxel_gt.{pdf,png}")
