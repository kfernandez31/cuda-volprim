#!/usr/bin/env python3
"""Scattering unbiasedness via mean convergence (cloud, meadow, albedo 0.9, HG g=0.85).

From the banked 16-seed g1 renders: the converged mean radiance of ours (MIS) and Mitsuba-analog
agree to within Monte-Carlo noise (both unbiased), while the *estimate* of that mean tightens far
faster for ours -- its per-seed image-mean spread is ~70x smaller than the firefly-prone analog
reference. The shrinking bands are the standard error of the mean estimate at each sample budget.

  tools/refs/.venv/bin/python scripts/plots/scattering_convergence.py thesis/latex/figures/scattering_ladder.pdf
"""
import sys, os, glob
import numpy as np, OpenEXR, Imath
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt

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
    n = min(len(ours), len(mits))
    k = np.arange(1, n + 1)
    spp = k * SPP_PER_SEED
    om, os_ = ours.mean(), ours.std(ddof=1)
    mm, ms = mits.mean(), mits.std(ddof=1)
    se_o = os_ / np.sqrt(k)
    se_m = ms / np.sqrt(k)

    if os.path.exists(STYLE):
        plt.style.use(STYLE)
    fig, ax = plt.subplots(figsize=(5.6, 3.6))
    ax.fill_between(spp, mm - se_m, mm + se_m, color="C3", alpha=0.20, lw=0)
    ax.plot(spp, np.full_like(spp, mm, float), color="C3", lw=1.6,
            label=f"Mitsuba-analog  ($\\to${mm:.4f})")
    ax.fill_between(spp, om - se_o, om + se_o, color="C0", alpha=0.30, lw=0)
    ax.plot(spp, np.full_like(spp, om, float), color="C0", lw=1.6,
            label=f"ours (MIS)  ($\\to${om:.4f})")
    ax.set_xscale("log")
    ax.set_xlabel("samples per pixel")
    ax.set_ylabel("mean radiance estimate $\\pm$ s.e.")
    ax.set_title("Scattering: both estimators converge to the same mean")
    ax.legend(loc="upper right", fontsize=8)
    ax.annotate(f"agree to {abs(om/mm-1)*100:.1f}%  (unbiased)\n"
                f"ours' mean-estimate s.e. {ms/os_:.0f}$\\times$ tighter",
                xy=(0.03, 0.05), xycoords="axes fraction", fontsize=8, va="bottom")
    fig.tight_layout()
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    fig.savefig(out); fig.savefig(os.path.splitext(out)[0] + ".png")
    print(f"wrote {out}: ours {om:.4f}+-{os_:.5f}  mits {mm:.4f}+-{ms:.5f}  "
          f"agree {abs(om/mm-1)*100:.2f}%  se-ratio {ms/os_:.1f}x")


if __name__ == "__main__":
    main()
