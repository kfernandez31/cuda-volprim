#!/usr/bin/env python3
"""Assemble fig:voxel-gt (thesis Sec 5.3) from the banked absorption cross-check artifacts.

Row 1 (production density): analytic Gaussian renderer | 600^3 dense-grid delta-tracking
render (Mitsuba heterogeneous/gridvolume, 32 spp, clamp 250) | |difference| x5.
Row 2 (4% density, sigma-mult 0.3 vs production 7.5): the same pair re-rendered where the
whole interior is visible (min T ~ 0.17); grid arm unclamped, 4096 spp; signed relative
difference at +-5% (thesis-wide convention). Inputs (results/campaign/):
  ico_fig/analytic.exr                                  production analytic arm
  voxgt_absorption_cloud_600_ss1_c250.exr               production grid arm
  reduced_density/thin_ours.exr                         4% analytic arm
  voxgt_absorption_cloud_600_ss1_c0_s0.04_seed0.exr     4% grid arm (seed 7 = noise floor)
Run under experiments/mitsuba-reference/.venv. Output: latex/figures/voxel_gt.{pdf,png}
"""
import os

import numpy as np
import mitsuba as mi

mi.set_variant("scalar_rgb")
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

RES = "results/campaign"


def load(path):
    return np.array(mi.Bitmap(path)).astype(np.float64)[..., :3]


tone = lambda a: np.clip(a, 0, 1) ** (1 / 2.2)


def signed_rel(a, b, scale=0.05):
    rel = (a.mean(-1) - b.mean(-1)) / np.maximum(b.mean(-1), 1e-3)
    t = np.clip(rel / scale, -1, 1)
    pos, neg = np.clip(t, 0, 1), np.clip(-t, 0, 1)
    return np.stack([1 - 0.85 * neg, 1 - 0.85 * pos - 0.55 * neg, 1 - 0.85 * pos], -1)


# --- row 1: production density (banked campaign artifacts, unchanged) ---
ours_p = load(f"{RES}/ico_fig/analytic.exr")
grid_p = load(f"{RES}/voxgt_absorption_cloud_600_ss1_c250.exr")
diff_p = np.abs(ours_p - grid_p)
print(f"production: analytic mean {ours_p.mean():.4f}  grid mean {grid_p.mean():.4f}  "
      f"median |diff| {np.median(diff_p):.4g}")

# --- row 2: 4% density ---
ours_t = load(f"{RES}/reduced_density/thin_ours.exr")
grid_t = load(f"{RES}/voxgt_absorption_cloud_600_ss1_c0_s0.04_seed0.exr")
ratio_t = float(ours_t.mean() / grid_t.mean())
rmse_t = float(np.sqrt(((ours_t - grid_t) ** 2).mean()))
line2 = f"ratio {ratio_t:.4f},  RMSE {rmse_t:.4f}"
seed7 = f"{RES}/voxgt_absorption_cloud_600_ss1_c0_s0.04_seed7.exr"
if os.path.exists(seed7):
    g7 = load(seed7)
    noise = float(np.sqrt(((grid_t - g7) ** 2).mean()) / np.sqrt(2))
    print(f"grid-arm noise floor (seed pair): {noise:.4f}")
print(f"4%: ours mean {ours_t.mean():.4f}  grid mean {grid_t.mean():.4f}  {line2}")
g128 = f"{RES}/voxgt_absorption_cloud_128_ss1_c0_s0.04_seed0.exr"
if os.path.exists(g128):
    r128 = float(ours_t.mean() / load(g128).mean())
    print(f"4% resolution convergence: 128^3 ratio {r128:.4f} -> 600^3 ratio {ratio_t:.4f}")

rows = [
    [(tone(ours_p), "analytic (this renderer)"),
     (tone(grid_p), r"$600^3$ grid, delta tracking"),
     (tone(diff_p * 5.0), r"$|\Delta| \times 5$")],
    [(tone(ours_t), None), (tone(grid_t), None),
     (signed_rel(ours_t, grid_t), r"signed rel. diff ($\pm$5%)")],
]
fig, axes = plt.subplots(2, 3, figsize=(10.5, 5.8))
for r, row in enumerate(rows):
    for ax, (img, title) in zip(axes[r], row):
        ax.imshow(img)
        if title:
            ax.set_title(title, fontsize=10)
        ax.set_xticks([]); ax.set_yticks([])
axes[0][0].set_ylabel("production density", fontsize=9)
axes[1][0].set_ylabel(r"4% density", fontsize=9)
axes[1][2].text(0.5, -0.06, line2, transform=axes[1][2].transAxes,
                ha="center", va="top", fontsize=10)
fig.tight_layout()
for ext in ("pdf", "png"):
    fig.savefig(f"latex/figures/voxel_gt.{ext}", dpi=130, bbox_inches="tight")
print("wrote latex/figures/voxel_gt.{pdf,png}")
