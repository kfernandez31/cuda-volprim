"""§7.6 scaling figure: time vs N for three constructed families, with the joint
two-term cost model  t = t0_f + a*N + b_f*crossings_f(N)  (a shared across families:
the bounce-0 containment scan; crossings = {const, N^(1/3) layers, N}).

Reads results/campaign/scaling_v2.csv (all repeats retained).
Usage: scaling_v2.py --out latex/figures/scaling_v2.pdf
"""
import argparse
import csv
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

STYLE = os.path.join(os.path.dirname(__file__), "style.mplstyle")
FAM = {
    "sheet": ("sheet: crossings constant", "#1f77b4", "o"),
    "cube": ("cube: crossings $\\propto N^{1/3}$", "#2ca02c", "s"),
    "stack": ("stack: crossings $= N$", "#d1495b", "^"),
}


def load(path):
    data = {}
    for r in csv.DictReader(open(path)):
        reps = [float(r[f"t{i}"]) for i in range(5) if r[f"t{i}"] not in ("", None)]
        data.setdefault(r["family"], []).append(
            (int(r["N"]), float(r["t_med_s"]), min(reps), max(reps)))
    return {k: sorted(v) for k, v in data.items()}


def joint_fit(data):
    """LSQ for [t0_sheet, t0_cube, t0_stack, a, b_cube, b_stack], shared a."""
    A, y = [], []
    for i, fam in enumerate(("sheet", "cube", "stack")):
        for N, t, _, _ in data[fam]:
            row = [0.0] * 6
            row[i] = 1.0
            row[3] = N
            if fam == "cube":
                row[4] = N ** (1 / 3)
            if fam == "stack":
                row[5] = N
            A.append(row)
            y.append(t)
    A, y = np.array(A), np.array(y)
    # minimise RELATIVE error (equal weight per point on log axes):
    # rows scaled by 1/t -> weighted linear LSQ, still exact
    w = 1.0 / y
    sol = np.linalg.lstsq(A * w[:, None], y * w, rcond=None)[0]
    return sol


def model_curve(fam, sol, N):
    t0 = {"sheet": sol[0], "cube": sol[1], "stack": sol[2]}[fam]
    t = t0 + sol[3] * N
    if fam == "cube":
        t = t + sol[4] * N ** (1 / 3)
    if fam == "stack":
        t = t + sol[5] * N
    return t


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", default="results/campaign/scaling_v2.csv")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    data = load(a.csv)
    sol = joint_fit(data)
    t0s, t0c, t0k, sa, bc, bk = sol

    if os.path.exists(STYLE):
        plt.style.use(STYLE)
    fig, ax = plt.subplots(figsize=(6.4, 3.8))

    for fam, (label, col, mk) in FAM.items():
        rows = data[fam]
        N = np.array([r[0] for r in rows], float)
        t = np.array([r[1] for r in rows], float)
        lo = np.array([r[2] for r in rows], float)
        hi = np.array([r[3] for r in rows], float)
        ax.errorbar(N, t, yerr=[t - lo, hi - t], fmt=mk, ms=4.0, lw=0,
                    elinewidth=0.9, capsize=2, color=col, label=label, zorder=3)
        gx = np.geomspace(N.min(), N.max(), 200)
        ax.plot(gx, model_curve(fam, sol, gx), "-", lw=1.2, color=col, alpha=0.8,
                zorder=2)
        rel = np.abs(model_curve(fam, sol, N) - t) / t
        print(f"{fam}: model median rel err {100*np.median(rel):.1f}%  "
              f"max {100*rel.max():.1f}%  (n={len(N)})")

    # slope guides
    ax.plot([16, 300], [0.155, 0.155], "k--", lw=0.9, alpha=0.55)
    ax.annotate("slope 0", (70, 0.146), fontsize=7.5, alpha=0.75, va="top")
    gx = np.array([64.0, 512.0])
    ax.plot(gx, 0.55 * (gx / 64.0), "k--", lw=0.9, alpha=0.55)
    ax.annotate("slope 1", (200, 2.6), fontsize=7.5, alpha=0.75)
    # second slope-1 guide along the sheet tail: the vertical offset between the two
    # slope-1 guides is the ~20x scan-vs-transport per-primitive cost ratio
    gx2 = np.array([1400.0, 9000.0])
    ax.plot(gx2, 0.40 * (gx2 / 1400.0), "k--", lw=0.9, alpha=0.55)
    ax.annotate("slope 1", (5200, 1.15), fontsize=7.5, alpha=0.75)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("primitive count $N$")
    ax.set_ylabel("render time (s)")
    ax.legend(fontsize=7.5, loc="upper left", title="lines: joint cost model",
              title_fontsize=7.5)
    fig.tight_layout()
    fig.savefig(a.out)
    print(f"t0 (s): sheet {t0s:.3f}, cube {t0c:.3f}, stack {t0k:.3f}")
    print(f"scan  a = {1e3*sa:.3g} ms/primitive (shared)")
    print(f"cube  b = {1e3*bc:.3g} ms/layer;  stack b = {1e3*bk:.3g} ms/crossed primitive")
    print(f"transport/scan per-primitive ratio (stack): {bk/sa:.0f}x")
    print(f"wrote {a.out}")


if __name__ == "__main__":
    main()
