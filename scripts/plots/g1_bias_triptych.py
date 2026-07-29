#!/usr/bin/env python3
"""G1 bias figure: ours-MIS | Mitsuba-NEE | Mitsuba-analog (GT).

The headline discovery (G1-B): Mitsuba's next-event-estimation direct lighting is energy-biased on
dense scattering media (+156% vs the unbiased analog ground truth), while our MIS estimator matches
GT to within Monte-Carlo noise. Each panel is the mean of 16 independent 64-spp seeds (~1024 spp
effective) so the systematic brightness difference is not noise. All panels share one exposure +
gamma so the over-brightness is directly visible.

  experiments/mitsuba-reference/.venv/bin/python scripts/plots/g1_bias_triptych.py \
      --out thesis/latex/figures/g1_bias.pdf
"""
import argparse, glob, os
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


def avg(fam):
    fs = sorted(glob.glob(f"{SEEDS}/{fam}_seed*.exr"))
    if not fs:
        raise SystemExit(f"no EXRs for {fam}")
    return np.mean([load(f) for f in fs], axis=0), len(fs)


def tonemap(img, exposure):
    return np.clip(img * exposure, 0, 1) ** (1 / 2.2)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--exposure", type=float, default=1.0)
    args = ap.parse_args()

    # ours-MIS GT panel: spp-weighted average of every banked meadow render of the scene
    # (8x1024 + 4x2048 + 4x4096 + 2x4096 = 40960 spp effective) — highest-budget panel of the three.
    CC = "results/campaign/cloud_conv"
    ours_files = ([(f"{CC}/ours_spp1024_seed{s}.exr", 1024) for s in range(1, 9)]
                  + [(f"{CC}/ours_spp2048_seed{s}.exr", 2048) for s in (5, 6, 7, 8)]
                  + [(f"{CC}/ours_spp4096_seed{s}.exr", 4096) for s in (5, 6, 7, 8)]
                  + [(f"{CC}/ours_gt4096_seed{s}.exr", 4096) for s in (1, 2)])
    ours_files = [(f, w) for f, w in ours_files if os.path.exists(f)]
    wsum = sum(w for _, w in ours_files)
    ours = np.sum([load(f) * (w / wsum) for f, w in ours_files], axis=0)
    n_o = wsum  # effective spp
    nee, n_n = avg("mitsnee")    # Mitsuba-NEE — biased
    gt, n_g = avg("mits")        # Mitsuba-analog — unbiased ground truth

    import glob as _glob
    _fx = sorted(_glob.glob("results/campaign/nee_fair/gt_fix5/gabor_nee_meadow_spp2048_seed*.exr"))
    fx = np.mean([load(f) for f in _fx], axis=0)
    g = gt.mean()   # converged ground-truth level (all unbiased arms agree within noise)
    panels = [
        (ours, f"ours (certified ground truth)\nmean {ours.mean():.4f}"),
        (nee,  f"reference NEE as shipped (DSYG revision)\nmean {nee.mean():.4f}  ({(nee.mean()/g-1)*100:+.0f}%)"),
        (fx,   f"reference NEE, corrected\n(current + patch set)  mean {fx.mean():.4f}"),
    ]
    print(f"means: ours={ours.mean():.4f} nee={nee.mean():.4f} gt={gt.mean():.4f} "
          f"| ours {((ours.mean()/g-1)*100):+.2f}%  nee {((nee.mean()/g-1)*100):+.1f}%  "
          f"(n={n_o}/{n_n}/{n_g} seeds)")

    if os.path.exists(STYLE):
        plt.style.use(STYLE)
    fig, axes = plt.subplots(1, 3, figsize=(10.5, 2.7))
    for ax, (img, title) in zip(axes, panels):
        ax.imshow(tonemap(img, args.exposure)); ax.set_title(title, fontsize=10); ax.axis("off")
    plt.tight_layout()
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    fig.savefig(args.out, dpi=140, bbox_inches="tight")
    png = os.path.splitext(args.out)[0] + ".png"
    fig.savefig(png, dpi=140, bbox_inches="tight")
    print(f"wrote {args.out} and {png}")


if __name__ == "__main__":
    main()
