#!/usr/bin/env python3
"""Furnace bias-vs-spp plot (advisor meeting 2026-06-25, Phase-0 gate).

Reads the long CSV (arm,sigma,spp,seed,mean,centre) from the furnace spp sweep, aggregates over seeds
into a mean centre-over-count with a 95% t-CI, and plots centre over-count (%) vs spp (log x), one line
per arm, one panel per sigma. A FLAT non-zero line (CI excluding 0) across spp = BIAS; a line decaying to
0 = convergence. Controls (ours-MIS, Mitsuba-analog) must sit on 0.

Usage:
  experiments/mitsuba-reference/.venv/bin/python scripts/plots/furnace_bias.py \
      --csv results/campaign/furnace_spp/furnace_bias_vs_spp.csv \
      --out latex/figures/furnace_bias_vs_spp.pdf
"""
import argparse, csv, os
from collections import defaultdict
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

_T95 = {2: 12.706, 3: 4.303, 4: 3.182, 5: 2.776, 6: 2.571, 7: 2.447, 8: 2.365,
        9: 2.306, 10: 2.262, 12: 2.201, 16: 2.131}  # t_{0.975, n-1}

def t95(n):
    return _T95.get(n, 1.96)

ap = argparse.ArgumentParser()
ap.add_argument("--csv", required=True)
ap.add_argument("--out", required=True)
a = ap.parse_args()

style = os.path.join(os.path.dirname(__file__), "style.mplstyle")
if os.path.exists(style):
    plt.style.use(style)

# group[(arm, sigma)][spp] = list of centre values
group = defaultdict(lambda: defaultdict(list))
arms, sigmas = [], []
for r in csv.DictReader(open(a.csv)):
    arm, sigma, spp = r["arm"], float(r["sigma"]), int(r["spp"])
    group[(arm, sigma)][spp].append(float(r["centre"]))
    if arm not in arms: arms.append(arm)
    if sigma not in sigmas: sigmas.append(sigma)
sigmas = sorted(sigmas)

LABEL = {"mits_nee": "Mitsuba NEE, shipped rev.\ (under test)", "mits_analog": "Mitsuba analog, shipped rev.\ (control)",
         "ours": "ours MIS (control)"}
COLOR = {"mits_nee": "#d1495b", "mits_analog": "#30638e", "ours": "#2e8b57"}
order = [x for x in ("mits_nee", "mits_analog", "ours") if x in arms]

fig, axes = plt.subplots(1, len(sigmas), figsize=(5.5 * len(sigmas), 3.4), sharey=False)
if len(sigmas) == 1:
    axes = [axes]

for ax, sigma in zip(axes, sigmas):
    for arm in order:
        spps = sorted(group[(arm, sigma)].keys())
        if not spps:
            continue
        ys, errs = [], []
        for spp in spps:
            v = np.array(group[(arm, sigma)][spp])
            oc = (v - 1.0) * 100.0  # centre over-count %
            ys.append(oc.mean())
            errs.append(t95(len(oc)) * oc.std(ddof=1) / np.sqrt(len(oc)) if len(oc) > 1 else 0.0)
        ax.errorbar(spps, ys, yerr=errs, marker="o", capsize=3,
                    color=COLOR.get(arm, None), label=LABEL.get(arm, arm))
    ax.axhline(0.0, color="0.4", lw=0.8, ls=":")
    ax.set_xscale("log", base=2)
    ax.set_xlabel("samples per pixel")
    ax.set_title(rf"$\sigma={sigma:g}$")
    ax.set_xticks(sorted({s for arm in order for s in group[(arm, sigma)].keys()}))
    ax.get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter())
axes[0].set_ylabel("furnace centre over-count (\\%)")
axes[0].legend(loc="center left", fontsize=8)
fig.tight_layout()
fig.savefig(a.out)
print(f"wrote {a.out}")

# also print the aggregated table for the record / decision rule
print(f"\n{'arm':14s} {'sigma':>6s} {'spp':>6s} {'centre%':>9s} {'95%CI':>14s}")
for sigma in sigmas:
    for arm in order:
        for spp in sorted(group[(arm, sigma)].keys()):
            v = np.array(group[(arm, sigma)][spp]); oc = (v - 1.0) * 100.0
            h = t95(len(oc)) * oc.std(ddof=1) / np.sqrt(len(oc)) if len(oc) > 1 else 0.0
            print(f"{arm:14s} {sigma:6g} {spp:6d} {oc.mean():8.3f}% [{oc.mean()-h:6.2f},{oc.mean()+h:6.2f}]")
