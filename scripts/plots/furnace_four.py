#!/usr/bin/env python3
"""Fig: furnace invariant across the four estimator arms (thesis naming convention).
Data: results/campaign/furnace_fix5/furnace4.csv (corrected-fork NEE/analog + ours-analog)
    + results/campaign/furnace_spp/furnace_bias_vs_spp.csv (ours-MIS rows, arm 'ours').
Usage: furnace_four.py --out latex/figures/furnace_four.pdf
"""
import argparse, csv, os
from collections import defaultdict
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
STYLE = os.path.join(HERE, "style.mplstyle")
ARMS = [("ours", "this renderer (MIS)", "#1f77b4"),
        ("ours_analog", "this renderer (analog)", "#9467bd"),
        ("ref_nee_corrected", "reference (corrected NEE)", "#ff7f0e"),
        ("ref_analog_corrected", "reference (analog)", "#2ca02c")]
SPPS = [64, 256, 1024, 4096]

def rows(path):
    with open(path) as f:
        yield from csv.DictReader(f)

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--out", required=True)
    a = ap.parse_args()
    data = defaultdict(list)   # (arm, sigma, spp) -> [centre...]
    for r in list(rows("results/campaign/furnace_fix5/furnace4.csv")) + \
             list(rows("results/campaign/furnace_spp/furnace_bias_vs_spp.csv")):
        arm = r["arm"]
        if arm not in {x[0] for x in ARMS}:
            continue
        sig = round(float(r["sigma"]))
        spp = int(r["spp"])
        if spp not in SPPS or sig not in (6, 12):
            continue
        data[(arm, sig, spp)].append(100 * (float(r["centre"]) - 1))
    if os.path.exists(STYLE):
        plt.style.use(STYLE)
    fig, axes = plt.subplots(1, 2, figsize=(9.0, 3.2), sharey=True)
    T95 = 2.365  # t(7)
    for ax, sig in zip(axes, (6, 12)):
        for k, (arm, label, col) in enumerate(ARMS):
            xs, ys, es = [], [], []
            for spp in SPPS:
                v = data.get((arm, sig, spp))
                if not v:
                    continue
                v = np.array(v)
                xs.append(spp * (1.0 + 0.06 * (k - 1.5)))  # slight x-dodge
                ys.append(v.mean())
                es.append(T95 * v.std(ddof=1) / np.sqrt(len(v)) if len(v) > 1 else 0.0)
            ax.errorbar(xs, ys, yerr=es, fmt="o-", ms=3.5, lw=1.2, capsize=2,
                        color=col, label=label)
        ax.axhline(0, color="k", lw=0.8, alpha=0.6)
        ax.set_xscale("log")
        ax.set_xlabel("samples per pixel")
        ax.set_title(f"$\\sigma = {sig}$", fontsize=10)
        ax.grid(True, which="both", alpha=0.25)
    axes[0].set_ylabel("furnace centre over-count (%)")
    axes[0].legend(fontsize=8, loc="upper left")
    axes[0].set_ylim(-0.09, 0.09)
    fig.tight_layout()
    fig.savefig(a.out)
    fig.savefig(os.path.splitext(a.out)[0] + ".png", dpi=140)
    print("wrote", a.out)

if __name__ == "__main__":
    main()
