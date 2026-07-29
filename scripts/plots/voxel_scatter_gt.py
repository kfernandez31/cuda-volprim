#!/usr/bin/env python3
"""Scattering voxel-GT figure: ours | AdVol dense-grid GT | amplified difference.

Independent cross-check of the renderer under SCATTERING (meadow env, albedo 0.9, HG g=0.85):
our analytic-Gaussian render vs a dense voxel grid of the same cloud rendered by AdVol (Jorge's
DSYG grid baseline) with local supervoxel majorants + residual ratio tracking -- which, unlike a
stock global-majorant tracker, is firefly-free at the cloud's full dynamic range (unclamped).

  experiments/mitsuba-reference/.venv/bin/python scripts/plots/voxel_scatter_gt.py --out thesis/latex/figures/voxel_scatter_gt.pdf
"""
import argparse, glob, os
import numpy as np, OpenEXR, Imath
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt

import os as _os
_MEAN = "results/campaign/advol_seeds/advol_cloud_200_ss1_mean.exr"
ADV = _MEAN if _os.path.exists(_MEAN) else "results/campaign/advol_seeds/advol_cloud_200_ss1_seed0.exr"
OURS = "results/campaign/g1_seeds/cuda_seed*.exr"


def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()["dataWindow"]
    w = dw.max.x - dw.min.x + 1; h = dw.max.y - dw.min.y + 1
    pt = Imath.PixelType(Imath.PixelType.FLOAT); ch = f.header()["channels"].keys()
    return np.stack([np.frombuffer(f.channel(c, pt), np.float32).reshape(h, w)
                     for c in ("R", "G", "B") if c in ch], -1)


def tonemap(img, e=1.3):
    return np.clip(img * e, 0, 1) ** (1 / 2.2)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--amp", type=float, default=5.0)
    args = ap.parse_args()

    ours = np.mean([load(f) for f in sorted(glob.glob(OURS))], 0)   # converged ours
    adv = load(ADV)
    rmse = float(np.sqrt(((adv - ours) ** 2).mean()))
    diff = np.clip(np.abs(adv - ours).mean(-1) * args.amp, 0, 1)
    print(f"ours {ours.mean():.4f}  AdVol-GT {adv.mean():.4f}  ratio {adv.mean()/ours.mean():.4f}  RMSE {rmse:.4f}")

    fig, axes = plt.subplots(1, 3, figsize=(10.2, 2.5))
    axes[0].imshow(tonemap(ours)); axes[0].set_title("ours (analytic Gaussians)", fontsize=9)
    axes[1].imshow(tonemap(adv)); axes[1].set_title(f"AdVol dense-grid GT (ratio {adv.mean()/ours.mean():.3f})", fontsize=9)
    im = axes[2].imshow(diff, cmap="magma"); axes[2].set_title(f"$|$diff$|\\times{args.amp:.0f}$  (RMSE {rmse:.3f})", fontsize=9)
    for a in axes:
        a.axis("off")
    plt.tight_layout()
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    fig.savefig(args.out, dpi=150, bbox_inches="tight")
    fig.savefig(os.path.splitext(args.out)[0] + ".png", dpi=150, bbox_inches="tight")
    print(f"wrote {args.out}")


if __name__ == "__main__":
    main()
