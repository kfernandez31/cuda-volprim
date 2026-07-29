#!/usr/bin/env python3
"""Icosphere N=3 sliver-artifact figure: analytic (exact) | icosphere N=3 | |difference|, with a
zoom inset on the worst region.

cloud, pure absorption (cloud_asset_validation, white_constant, 64 spp) → transmittance is
deterministic, so every difference is geometric, not Monte-Carlo noise. At fine tessellation the
icosphere triangles become slivers; near-grazing rays land a faceted entry far from the true surface
and the analytic exit then mis-integrates a localised population of rays (cloud N=3: ~3150 px at
|Δ|>0.05, max |Δ|≈0.25). The analytic built-in sphere is exact regardless — the quality argument for
the analytic primitive (see icosphere_port.md / G8).

  experiments/mitsuba-reference/.venv/bin/python scripts/plots/icosphere_sliver.py --out latex/figures/icosphere_sliver.pdf
"""
import argparse, os
import numpy as np, OpenEXR, Imath
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle

HERE = os.path.dirname(os.path.abspath(__file__))
STYLE = os.path.join(HERE, "style.mplstyle")
DIR = "results/campaign/ico_v2"   # 2026-07-13 accuracy re-run (fresh builds, l=0..4)
AMP = 4
CY, CX, CS = 400, 0, 120  # worst-region crop (from diff scan)


def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()["dataWindow"]
    w = dw.max.x - dw.min.x + 1; h = dw.max.y - dw.min.y + 1
    pt = Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c, pt), np.float32).reshape(h, w)
                     for c in ("R", "G", "B")], -1)


def tonemap(img):
    return np.clip(img, 0, 1) ** (1 / 2.2)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    a = load(f"{DIR}/cloud_analytic.exr")
    i = load(f"{DIR}/cloud_icoN3.exr")

    def signed_diff(img):
        rel = (img.mean(-1) - a.mean(-1)) / np.maximum(a.mean(-1), 1e-3)
        t = np.clip(rel / 0.10, -1, 1)   # +-10% full scale (thesis diff convention, white midpoint)
        pos, neg = np.clip(t, 0, 1), np.clip(-t, 0, 1)
        return np.stack([1 - 0.85 * neg, 1 - 0.85 * pos - 0.55 * neg, 1 - 0.85 * pos], -1)

    d = signed_diff(i)
    rmse = float(np.sqrt(((a - i) ** 2).mean()))
    npx = int((np.abs(a - i).max(-1) > 0.05).sum())
    print(f"RMSE={rmse:.3e}  px|Δ|>0.05={npx}  max|Δ|={np.abs(a-i).max():.3f}")

    if os.path.exists(STYLE):
        plt.style.use(STYLE)
    fig = plt.figure(figsize=(10.5, 4.6))
    outer = fig.add_gridspec(2, 1, height_ratios=[2.0, 1.05], hspace=0.30)
    gs_top = outer[0].subgridspec(1, 3, wspace=0.05)
    gs_bot = outer[1].subgridspec(1, 4, wspace=0.08)
    axes = [fig.add_subplot(gs_top[0, k]) for k in range(3)]
    panels = [
        (tonemap(a), "analytic sphere (exact)"),
        (tonemap(i), f"icosphere $\\ell=3$ (1280 tris)\nRMSE ${rmse*1e3:.1f}\\times10^{{-3}}$, {npx} px $|\\Delta|>0.05$"),
        (d, "signed rel. diff\n(\u00b110%; geometric, no MC noise)"),
    ]
    for ax, (img, title) in zip(axes, panels):
        ax.imshow(img); ax.set_title(title, fontsize=8.5); ax.axis("off")
        ax.add_patch(Rectangle((CX, CY), CS, CS, ec="C1", fc="none", lw=1.0))
        axin = ax.inset_axes([0.60, 0.02, 0.38, 0.38])
        axin.imshow(img[CY:CY + CS, CX:CX + CS])
        for s in axin.spines.values():
            s.set_edgecolor("C1"); s.set_linewidth(1.0)
        axin.set_xticks([]); axin.set_yticks([])

    # bottom strip: the same crop's signed diff across l=1..4 \u2014 the reversal in one row
    for k, lev in enumerate((1, 2, 3, 4)):
        img = load(f"{DIR}/cloud_icoN{lev}.exr")
        dd = signed_diff(img)[CY:CY + CS, CX:CX + CS]
        cnt = int((np.abs(img - a).max(-1) > 0.05).sum())
        ax = fig.add_subplot(gs_bot[0, k])
        ax.imshow(dd); ax.set_xticks([]); ax.set_yticks([])
        for s in ax.spines.values():
            s.set_edgecolor("C1"); s.set_linewidth(0.8)
        ax.set_title(f"$\\ell={lev}$: {cnt} px $|\\Delta|>0.05$", fontsize=8)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    fig.savefig(args.out, dpi=140, bbox_inches="tight")
    png = os.path.splitext(args.out)[0] + ".png"
    fig.savefig(png, dpi=140, bbox_inches="tight")
    print(f"wrote {args.out} and {png}")


if __name__ == "__main__":
    main()
