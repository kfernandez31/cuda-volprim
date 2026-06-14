#!/usr/bin/env python3
"""Combined-showcase money shot (fig:showcase, sec:money-shot).

A SINGLE 64-spp realization from each renderer at matched render time (~9 s) on the
environment-lit cloud (measured HDR meadow, Henyey-Greenstein g=0.85):
  LEFT  -- ours (MIS): converged and firefly-free.
  RIGHT -- Mitsuba's only unbiased mode (analog): dominated by fireflies at the same budget.
Single seeds (not the 16-seed mean) on purpose -- the point is the per-frame firefly
character, which averaging would erase. The two agree on the converged image (means
0.321 vs 0.320, Ch. Results), so the contrast is variance, not bias.

  tools/refs/.venv/bin/python scripts/plots/showcase.py --out thesis/latex/figures/showcase.pdf
"""
import argparse, os
import numpy as np, OpenEXR, Imath
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
STYLE = os.path.join(HERE, "style.mplstyle")
SEEDS = "results/campaign/g1_seeds"


def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()["dataWindow"]
    w = dw.max.x - dw.min.x + 1; h = dw.max.y - dw.min.y + 1
    pt = Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c, pt), np.float32).reshape(h, w)
                     for c in ("R", "G", "B")], -1)


def tonemap(img, exposure):
    return np.clip(img * exposure, 0, 1) ** (1 / 2.2)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--exposure", type=float, default=1.3)
    ap.add_argument("--seed", default="00")
    args = ap.parse_args()

    ours = load(f"{SEEDS}/cuda_seed{args.seed}.exr")    # ours-MIS, one 64-spp frame
    mits = load(f"{SEEDS}/mits_seed{args.seed}.exr")     # Mitsuba-analog, one 64-spp frame

    # firefly count (pixels far above the converged mean) -- for the caption/log
    mu = mits.mean()
    ff = int((mits.max(-1) > 20 * mu).sum())
    print(f"ours mean={ours.mean():.3f} max={ours.max():.1f} | "
          f"mits mean={mits.mean():.3f} max={mits.max():.1f} | mits fireflies(>20x mean)={ff}")

    if os.path.exists(STYLE):
        plt.style.use(STYLE)
    fig, axes = plt.subplots(1, 2, figsize=(9.0, 3.1))
    panels = [
        (ours, "ours (MIS), 64 spp $\\approx$ 9 s"),
        (mits, "Mitsuba (analog), 64 spp $\\approx$ 9 s"),
    ]
    for ax, (img, title) in zip(axes, panels):
        ax.imshow(tonemap(img, args.exposure)); ax.set_title(title, fontsize=9); ax.axis("off")
    plt.tight_layout()
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    fig.savefig(args.out, dpi=150, bbox_inches="tight")
    png = os.path.splitext(args.out)[0] + ".png"
    fig.savefig(png, dpi=150, bbox_inches="tight")
    print(f"wrote {args.out} and {png}")


if __name__ == "__main__":
    main()
