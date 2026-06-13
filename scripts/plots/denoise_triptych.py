#!/usr/bin/env python3
"""Denoiser figure: noisy (16 spp) | OptiX-denoised (16 spp) | reference (~1024 spp).

cloud-meadow, MIS, calibrated caps. The reference is the mean of the 16 banked g1 ours-MIS seeds
(=1024 spp, same scene/config). RMSE-vs-reference annotates the noise reduction; the denoiser cuts
RMSE sharply but is a post-process (it blurs fine structure and is not unbiased), so it is reported as
a practical option, not part of the unbiased pipeline.

  tools/refs/.venv/bin/python scripts/plots/denoise_triptych.py --out thesis/latex/figures/denoise.pdf
"""
import argparse, glob, os
import numpy as np, OpenEXR, Imath
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
STYLE = os.path.join(HERE, "style.mplstyle")


def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()["dataWindow"]
    w = dw.max.x - dw.min.x + 1; h = dw.max.y - dw.min.y + 1
    pt = Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c, pt), np.float32).reshape(h, w)
                     for c in ("R", "G", "B")], -1)


def rmse(a, b):
    return float(np.sqrt(np.mean((a - b) ** 2)))


def tonemap(img):
    return np.clip(img, 0, 1) ** (1 / 2.2)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    noisy = load("results/campaign/denoise/noisy.exr")
    den = load("results/campaign/denoise/denoised.exr")
    gt = np.mean([load(f) for f in sorted(glob.glob("results/campaign/g1_seeds/cuda_seed*.exr"))], 0)

    r_n, r_d = rmse(noisy, gt), rmse(den, gt)
    print(f"RMSE-vs-ref: noisy={r_n:.4f}  denoised={r_d:.4f}  ({r_n/r_d:.1f}x lower)")

    if os.path.exists(STYLE):
        plt.style.use(STYLE)
    fig, axes = plt.subplots(1, 3, figsize=(10.5, 2.6))
    panels = [
        (noisy, f"path-traced (16 spp)\nRMSE {r_n:.3f}"),
        (den,   f"OptiX denoised (16 spp)\nRMSE {r_d:.3f}  ({r_n/r_d:.1f}$\\times$ lower)"),
        (gt,    "reference (~1024 spp)"),
    ]
    for ax, (img, title) in zip(axes, panels):
        ax.imshow(tonemap(img)); ax.set_title(title, fontsize=8.5); ax.axis("off")
    plt.tight_layout()
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    fig.savefig(args.out, dpi=140, bbox_inches="tight")
    png = os.path.splitext(args.out)[0] + ".png"
    fig.savefig(png, dpi=140, bbox_inches="tight")
    print(f"wrote {args.out} and {png}")


if __name__ == "__main__":
    main()
