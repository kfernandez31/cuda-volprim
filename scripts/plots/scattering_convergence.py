#!/usr/bin/env python3
"""Scattering unbiasedness via running-mean convergence (cloud, meadow, albedo 0.9, HG g=0.85).

From the banked 16-seed g1 renders (64 spp each): as independent seeds accumulate, the running mean
of ours (MIS) and of the Mitsuba-analog reference converge to the same value (both unbiased), but
ours snaps to it from the first seed while the firefly-prone analog wiggles in. The faint dots are the
per-seed estimates being averaged -- ours forms a tight band, the analog scatters ~68x wider.

  tools/refs/.venv/bin/python scripts/plots/scattering_convergence.py thesis/latex/figures/scattering_ladder.pdf
"""
import sys, os, glob
import numpy as np, OpenEXR, Imath
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

HERE = os.path.dirname(os.path.abspath(__file__)); STYLE = os.path.join(HERE, "style.mplstyle")
SEEDS = "results/campaign/g1_seeds"; SPP_PER_SEED = 64


def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()["dataWindow"]
    w = dw.max.x - dw.min.x + 1; h = dw.max.y - dw.min.y + 1
    pt = Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c, pt), np.float32).reshape(h, w)
                     for c in ("R", "G", "B")], -1)


def seed_means(fam):
    return np.array([load(f).mean() for f in sorted(glob.glob(f"{SEEDS}/{fam}_seed*.exr"))])


def main():
    out = sys.argv[1]
    ours = seed_means("cuda")      # ours-MIS
    mits = seed_means("mits")      # Mitsuba-analog
    n = min(len(ours), len(mits)); ours, mits = ours[:n], mits[:n]
    k = np.arange(1, n + 1)
    run_o = np.cumsum(ours) / k
    run_m = np.cumsum(mits) / k
    om, mm = run_o[-1], run_m[-1]
    ratio = mits.std(ddof=1) / ours.std(ddof=1)

    if os.path.exists(STYLE):
        plt.style.use(STYLE)
    fig, ax = plt.subplots(figsize=(5.6, 3.6))

    # Per-seed estimates being averaged (faint) -- the raw scatter.
    ax.scatter(k, mits, s=16, color="C3", alpha=0.45, lw=0, zorder=2)
    ax.scatter(k, ours, s=16, color="C0", alpha=0.55, lw=0, zorder=2)

    # Running means (solid) -- the convergence.
    ax.plot(k, run_m, color="C3", lw=1.8, marker="o", ms=3, zorder=4,
            label=f"Mitsuba-analog, running mean ($\\to${mm:.4f})")
    ax.plot(k, run_o, color="C0", lw=1.8, marker="o", ms=3, zorder=4,
            label=f"ours (MIS), running mean ($\\to${om:.4f})")

    ax.axhline(0.5 * (om + mm), color="0.4", lw=0.9, ls="--", zorder=1)

    ax.set_xlim(0.5, n + 0.5)
    ax.set_xlabel("independent seeds averaged (64 spp each)")
    ax.set_ylabel("mean radiance estimate")
    ax.set_title("Scattering: running means converge to one value")
    handles, labels = ax.get_legend_handles_labels()
    handles.append(Line2D([], [], marker="o", ls="none", color="0.5", alpha=0.6, ms=4))
    labels.append("per-seed estimates (64 spp each)")
    ax.legend(handles, labels, loc="upper right", fontsize=8)
    ax.annotate(f"converge to within {abs(om/mm-1)*100:.1f}% (both unbiased)\n"
                f"per-seed scatter {ratio:.0f}$\\times$ tighter for ours",
                xy=(0.03, 0.04), xycoords="axes fraction", fontsize=8, va="bottom")

    fig.tight_layout()
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    fig.savefig(out); fig.savefig(os.path.splitext(out)[0] + ".png")
    print(f"wrote {out}: ours→{om:.4f} mits→{mm:.4f} agree {abs(om/mm-1)*100:.2f}% "
          f"per-seed ratio {ratio:.1f}x")


if __name__ == "__main__":
    main()
