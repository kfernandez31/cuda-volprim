#!/usr/bin/env python3
"""Roofline plot (log-log) for the render kernel vs the RTX 3090 roofs.

Framed strictly as a NON-SATURATION argument: the kernel's point sits far below both
the memory roof and the compute roof, so it is neither bandwidth- nor compute-bound --
it is latency/divergence-bound (see ncu_summary.md). The GFLOP/s value undercounts the
erf/transcendental (XU-pipe) work, which only strengthens the argument.

  tools/refs/.venv/bin/python scripts/plots/roofline.py \
      --csv results/campaign/roofline.csv --out thesis/latex/figures/roofline.pdf
"""
import argparse
import os

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
STYLE = os.path.join(HERE, "style.mplstyle")

# RTX 3090 roofs (GA102): FP32 peak ~35.6 TFLOP/s, DRAM peak ~936 GB/s.
PEAK_GFLOPS = 35_580.0
PEAK_BW_GBS = 936.0


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", required=True, help="kernel, arith_intensity, achieved_gflops")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    data = np.genfromtxt(args.csv, delimiter=",", names=True, dtype=None, encoding="utf-8")
    data = np.atleast_1d(data)

    if os.path.exists(STYLE):
        plt.style.use(STYLE)
    fig, ax = plt.subplots()

    ai = np.logspace(-1, 3, 256)
    roof = np.minimum(PEAK_GFLOPS, ai * PEAK_BW_GBS)
    ax.plot(ai, roof, color="0.3", lw=1.5)
    ridge = PEAK_GFLOPS / PEAK_BW_GBS
    ax.annotate("memory roof\n(936 GB/s)", xy=(0.6, 0.6 * PEAK_BW_GBS), fontsize=8,
                rotation=38, ha="center", va="bottom", color="0.3")
    ax.annotate(f"compute roof ({PEAK_GFLOPS/1000:.1f} TFLOP/s FP32)",
                xy=(ridge * 6, PEAK_GFLOPS * 1.15), fontsize=8, ha="left", color="0.3")

    for row in data:
        x, y = float(row["arith_intensity"]), float(row["achieved_gflops"])
        ax.scatter([x], [y], zorder=5, s=45, color="C3")
        lbl = str(row["kernel"]).replace("render_megakernel_", "").replace("_", " · ")
        ax.annotate(lbl, xy=(x, y), xytext=(x * 1.25, y * 1.6), fontsize=9)
        # dotted guides up to each roof to visualise the head-room
        ax.plot([x, x], [y, min(PEAK_GFLOPS, x * PEAK_BW_GBS)], ls=":", color="C3", lw=1)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlim(0.1, 1000)
    ax.set_ylim(10, PEAK_GFLOPS * 3)
    ax.set_xlabel("arithmetic intensity (FLOP/byte)")
    ax.set_ylabel("achieved GFLOP/s")
    ax.set_title("Roofline (RTX 3090): far under both roofs")

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    fig.savefig(args.out)
    print(f"wrote {args.out}")


if __name__ == "__main__":
    main()
