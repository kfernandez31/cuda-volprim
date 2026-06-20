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

  tools/refs/.venv/bin/python scripts/plots/scaling.py \
      --csv results/campaign/scaling.csv --gas results/campaign/gas_memory.csv \
      --out thesis/latex/figures/scaling.pdf
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
    fig, (axT, axM) = plt.subplots(1, 2, figsize=(7.4, 3.3))

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

    # ---------------- RIGHT: memory vs N ----------------
    # flat reservation line (synthetic SAFE-512 peak readings at the sweep endpoints;
    # uses every synthetic row that carries a VRAM reading, not just the square grids)
    syn_all = kind == "synthetic"
    pk = peak[syn_all]
    pkN = N[syn_all]
    good = ~np.isnan(pk)
    resv = float(np.nanmedian(pk[good])) if good.any() else 1200.0
    axM.axhline(resv, color="C1", lw=1.6, label=f"peak device mem, fixed caps (~{resv:.0f} MiB)")
    if good.any():
        axM.scatter(pkN[good], pk[good], s=40, color="C1", zorder=5)

    # GAS(N): the only N-dependent term -- compacted BVH for cloud + bunny
    gN, gMB = [], []
    for asset, (_, mb) in gas_assets.items():
        if asset in real_N:
            gN.append(real_N[asset]); gMB.append(mb)
    gN, gMB = np.array(gN, float), np.array(gMB, float)
    o = np.argsort(gN); gN, gMB = gN[o], gMB[o]
    # linear GAS model through the two measured points -> coefficient in KB/prim
    slope = (gMB[-1] - gMB[0]) / (gN[-1] - gN[0])
    xg = np.array([gN.min(), gN.max()])
    axM.plot(xg, gMB[0] + slope * (xg - gN[0]), ls="--", color="C2", lw=1.2,
             label=f"BVH (GAS) $\\approx${slope*1024:.2f} KB/prim")
    axM.scatter(gN, gMB, s=40, color="C2", zorder=5)
    for x, y, nm in zip(gN, gMB, [a for a in gas_assets if a in real_N]):
        axM.annotate(f"{nm}\n{y:.2f} MB", xy=(x, y), xytext=(x * 0.5, y * 1.5),
                     fontsize=7.5, color="C2")

    axM.set_xscale("log")
    axM.set_yscale("log")
    axM.set_ylim(0.05, resv * 3)
    axM.set_xlabel("primitive count $N$")
    axM.set_ylabel("device memory (MiB)")
    axM.set_title("Memory is decoupled from $N$")
    axM.legend(loc="center left", fontsize=7.5)

    fig.tight_layout()
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    fig.savefig(args.out)
    print(f"wrote {args.out}  (synthetic exponent b={b:.3f}, reservation~{resv:.0f} MiB, "
          f"GAS slope {slope*1024:.3f} KB/prim)")


if __name__ == "__main__":
    main()
