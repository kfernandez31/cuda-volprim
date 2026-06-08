#!/usr/bin/env python3
"""Generate a publication figure (PDF) from an experiment CSV.

Run with the repo venv that has numpy + matplotlib:
  tools/refs/.venv/bin/python scripts/plots/figure_from_csv.py \
      --csv results/optim/rr_depth.csv --x rr_depth --y rmse time \
      --xlabel "RR depth" --title "RR-depth sweep" \
      --out thesis/latex/figures/rr_depth.pdf

The CSV must have a header row; --x and --y name columns by header.
Format-agnostic on purpose: the experiment harness (plan Task 0.3) defines the
columns; this tool just plots whichever ones you name.
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
    ap = argparse.ArgumentParser(description="CSV -> publication figure (PDF).")
    ap.add_argument("--csv", required=True)
    ap.add_argument("--x", required=True, help="x-axis column (header name)")
    ap.add_argument("--y", required=True, nargs="+", help="one or more y columns")
    ap.add_argument("--kind", default="line", choices=["line", "scatter", "bar"])
    ap.add_argument("--xlabel", default=None)
    ap.add_argument("--ylabel", default=None)
    ap.add_argument("--title", default=None)
    ap.add_argument("--logx", action="store_true")
    ap.add_argument("--logy", action="store_true")
    ap.add_argument("--out", required=True, help="output path (.pdf)")
    args = ap.parse_args()

    data = np.genfromtxt(args.csv, delimiter=",", names=True, dtype=None, encoding="utf-8")
    if data.ndim == 0:  # a single data row comes back 0-d
        data = data.reshape(1)
    cols = data.dtype.names or ()
    if args.x not in cols:
        raise SystemExit(f"x column '{args.x}' not in CSV columns {list(cols)}")
    missing = [c for c in args.y if c not in cols]
    if missing:
        raise SystemExit(f"y column(s) {missing} not in CSV columns {list(cols)}")

    x = np.atleast_1d(data[args.x])
    if os.path.exists(STYLE):
        plt.style.use(STYLE)
    fig, ax = plt.subplots()

    for yc in args.y:
        y = np.atleast_1d(data[yc])
        if args.kind == "line":
            ax.plot(x, y, marker="o", label=yc)
        elif args.kind == "scatter":
            ax.scatter(x, y, label=yc)
        else:  # bar — positional, with x values as tick labels
            pos = np.arange(len(x))
            ax.bar(pos, y, label=yc)
            ax.set_xticks(pos)
            ax.set_xticklabels([str(v) for v in x])

    if args.logx:
        ax.set_xscale("log")
    if args.logy:
        ax.set_yscale("log")
    ax.set_xlabel(args.xlabel or args.x)
    ax.set_ylabel(args.ylabel or (args.y[0] if len(args.y) == 1 else "value"))
    if args.title:
        ax.set_title(args.title)
    if len(args.y) > 1:
        ax.legend()

    out_dir = os.path.dirname(os.path.abspath(args.out))
    os.makedirs(out_dir, exist_ok=True)
    fig.savefig(args.out)
    print(f"wrote {args.out}  ({len(x)} points, y={args.y})")


if __name__ == "__main__":
    main()
