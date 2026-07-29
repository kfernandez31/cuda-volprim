#!/usr/bin/env python3
"""Scaling figure (two panels): render time and device memory vs primitive count N.

LEFT  -- time(N), log-log. Two series:
  * synthetic stress grids (SAFE-512 binary: CONSTANT caps, identical primitive kind,
    only N varies) -> the clean N-isolation curve, with a fitted power-law exponent.
  * the four real assets at their calibrated operating points (matched render config).
RIGHT -- memory(N), log-x / log-y. The point is DECOUPLING:
  * peak device memory at fixed caps is flat in N (per-ray local-mem reservation
    dominates) -- the synthetic SAFE-512 endpoints.
  * the BVH (GAS) is the only N-dependent term; it grows ~linearly but stays ~3 orders
    of magnitude below the reservation (cloud 0.10 MB @652 -> bunny 3.97 MB @25600).

  experiments/mitsuba-reference/.venv/bin/python scripts/plots/scaling.py \
      --csv results/campaign/scaling.csv --gas results/campaign/gas_memory.csv \
      --out latex/figures/scaling.pdf
"""
import argparse
import os

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
STYLE = os.path.join(HERE, "style.mplstyle")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", required=True, help="kind,label,N,t_med_s,t0,t1,t2,peak_mib")
    ap.add_argument("--gas", required=True, help="asset,gas_mb_uncompacted,gas_mb_compacted")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    rows = np.genfromtxt(args.csv, delimiter=",", names=True, dtype=None, encoding="utf-8")
    rows = np.atleast_1d(rows)
    kind = np.array([str(r["kind"]) for r in rows])
    N = np.array([float(r["N"]) for r in rows])
    t = np.array([float(r["t_med_s"]) for r in rows])
    label = np.array([str(r["label"]) for r in rows])
    def _peak(v):
        try:
            x = float(v)
            return x if x > 0 else np.nan   # empty cells arrive as -1 / 0
        except (TypeError, ValueError):
            return np.nan
    peak = np.array([_peak(r["peak_mib"]) for r in rows])

    # Synthetic stress grids are k x m grids; only the SQUARE (k x k) grids hold the
    # screen-coverage layout fixed so that N scales cleanly (the 1:2 rectangles change
    # aspect ratio and per-ray coverage, confounding N). Keep perfect-square N only.
    is_sq = np.array([abs(round(np.sqrt(n)) ** 2 - n) < 0.5 for n in N])
    syn = (kind == "synthetic") & is_sq
    real = kind == "asset"

    gas = np.genfromtxt(args.gas, delimiter=",", names=True, dtype=None, encoding="utf-8")
    gas = np.atleast_1d(gas)
    gas_assets = {str(g["asset"]): (None, float(g["gas_mb_compacted"])) for g in gas}
    # N per gas asset from the real rows
    real_N = {label[i].lower(): N[i] for i in range(len(N)) if real[i]}

    if os.path.exists(STYLE):
        plt.style.use(STYLE)
    fig, axT = plt.subplots(1, 1, figsize=(4.6, 3.3))

    # ---------------- LEFT: time vs N ----------------
    # synthetic power-law fit (log-log slope) over the stress grids
    Ns, ts = N[syn], t[syn]
    order = np.argsort(Ns)
    Ns, ts = Ns[order], ts[order]
    b, a = np.polyfit(np.log(Ns), np.log(ts), 1)  # log t = b log N + a
    xfit = np.array([Ns.min(), Ns.max()])
    axT.plot(xfit, np.exp(a) * xfit ** b, ls="--", color="0.5", lw=1.2, zorder=1,
             label=f"power-law fit  $t\\propto N^{{{b:.2f}}}$")
    axT.scatter(Ns, ts, s=44, color="C0", zorder=4,
                label="square grid (identical Gaussians)")
    # 1:2 rectangular grids: shown but excluded from the fit (aspect/coverage confounds N)
    rect = (kind == "synthetic") & ~is_sq
    if rect.any():
        axT.scatter(N[rect], t[rect], s=44, facecolors="none", edgecolors="0.55", zorder=3,
                    label="rectangular grid (excluded from fit)")
    # Real assets are NOT plotted as a scaling series: they confound N with packing
    # density and extent (Table~\ref{tab:overlap}); their costs are tabulated separately.

    axT.set_xscale("log")
    axT.set_yscale("log")
    axT.set_xlabel("primitive count $N$")
    axT.set_ylabel("render time (s)  [512$^2$, 64 spp]")
    axT.set_title("Time vs primitive count")
    axT.legend(loc="upper left", fontsize=7.5)

    fig.tight_layout()
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    fig.savefig(args.out)
    print(f"wrote {args.out}  (synthetic exponent b={b:.3f})")


if __name__ == "__main__":
    main()
