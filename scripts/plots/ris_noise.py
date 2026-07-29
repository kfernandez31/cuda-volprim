#!/usr/bin/env python3
"""RIS equal-quality noise figure (meadow env): MIS | RIS K=6 | converged reference, with a firefly
crop inset and per-arm RMSE-vs-reference annotations.

Each single-frame panel is one representative 64-spp seed (the one whose RMSE is the median across the
16 banked seeds, so it is neither best- nor worst-case). The reference is the mean of all 32 banked
frames (16 MIS + 16 RIS, ~2048 spp); MIS and RIS are both unbiased under environment lighting so they
converge to the same image. RIS K=6 reaches a lower RMSE *and* renders faster per sample (7.8 s vs
9.8 s, from the banked K-sweep CSV), so the equal-quality speedup (1.48x at this env, see fig:ris-ksweep) is the product of
both effects.

  experiments/mitsuba-reference/.venv/bin/python scripts/plots/ris_noise.py --out latex/figures/ris_noise.pdf
"""
import argparse, glob, os
import numpy as np, OpenEXR, Imath
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle

HERE = os.path.dirname(os.path.abspath(__file__))
STYLE = os.path.join(HERE, "style.mplstyle")
SEEDS = "results/campaign/ris_seeds_meadow"


def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()["dataWindow"]
    w = dw.max.x - dw.min.x + 1; h = dw.max.y - dw.min.y + 1
    pt = Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c, pt), np.float32).reshape(h, w)
                     for c in ("R", "G", "B")], -1)


def stack(prefix):
    fs = sorted(glob.glob(f"{SEEDS}/{prefix}_s*.exr"))
    if not fs:
        raise SystemExit(f"no EXRs for {prefix}")
    return np.array([load(f) for f in fs])


def rmse(a, b):
    return float(np.sqrt(np.mean((a - b) ** 2)))


def tonemap(img):
    return np.clip(img, 0, 1) ** (1 / 2.2)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    mis = stack("mis")    # MIS baseline, 16 x 64spp
    ris = stack("6")      # RIS K=6,       16 x 64spp
    ref = np.concatenate([mis, ris]).mean(axis=0)  # ~2048 spp, shared GT

    # representative = median-RMSE seed per arm (not best/worst)
    mis_rmse = np.array([rmse(f, ref) for f in mis])
    ris_rmse = np.array([rmse(f, ref) for f in ris])
    mis_pick = mis[np.argsort(mis_rmse)[len(mis) // 2]]
    ris_pick = ris[np.argsort(ris_rmse)[len(ris) // 2]]
    print(f"meadow RMSE-vs-ref: MIS={mis_rmse.mean():.4f}±{mis_rmse.std(ddof=1):.4f}  "
          f"RIS-K6={ris_rmse.mean():.4f}±{ris_rmse.std(ddof=1):.4f}  (n=16 each)")

    # firefly crop: brightest 96x96 window of the reference
    h, w, _ = ref.shape
    lum = ref.mean(-1)
    cs = 96
    ii = np.unravel_index(np.argmax(
        np.add.reduceat(np.add.reduceat(lum, np.arange(0, h, cs), 0), np.arange(0, w, cs), 1)),
        (len(np.arange(0, h, cs)), len(np.arange(0, w, cs))))
    y0, x0 = min(ii[0] * cs, h - cs), min(ii[1] * cs, w - cs)

    # panel times from the banked K-sweep record (t_med column), not hardcoded
    import csv
    with open("results/campaign/ris_ksweep_meadow.csv") as f:
        rows = list(csv.DictReader(f))
    t_mis = float(next(r["t_med_s"] for r in rows if r["arm"] == "mis"))
    t_ris = float(next(r["t_med_s"] for r in rows if r["arm"] == "ris" and r["K"] == "6"))

    if os.path.exists(STYLE):
        plt.style.use(STYLE)
    fig, axes = plt.subplots(1, 3, figsize=(10.5, 2.6))
    panels = [
        (mis_pick, f"MIS (64 spp)\nRMSE {mis_rmse.mean():.3f} ± {mis_rmse.std(ddof=1):.4f} (16 seeds), {t_mis:.1f} s"),
        (ris_pick, f"RIS K=6 (64 spp)\nRMSE {ris_rmse.mean():.3f} ± {ris_rmse.std(ddof=1):.4f} (16 seeds), {t_ris:.1f} s"),
        (ref,      "reference (~2048 spp)"),
    ]
    for ax, (img, title) in zip(axes, panels):
        ax.imshow(tonemap(img)); ax.set_title(title, fontsize=10); ax.axis("off")
        ax.add_patch(Rectangle((x0, y0), cs, cs, ec="C1", fc="none", lw=1.0))
        # firefly-crop inset (upper-right)
        axin = ax.inset_axes([0.62, 0.62, 0.36, 0.36])
        axin.imshow(tonemap(img[y0:y0 + cs, x0:x0 + cs]))
        for s in axin.spines.values():
            s.set_edgecolor("C1"); s.set_linewidth(1.0)
        axin.set_xticks([]); axin.set_yticks([])
    plt.tight_layout()
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    fig.savefig(args.out, dpi=140, bbox_inches="tight")
    png = os.path.splitext(args.out)[0] + ".png"
    fig.savefig(png, dpi=140, bbox_inches="tight")
    print(f"wrote {args.out} and {png}")


if __name__ == "__main__":
    main()
