#!/usr/bin/env python3
"""Generate a watermarked placeholder figure (PDF) for a not-yet-run campaign plot.

Produces an honest stand-in: NO fabricated data, just the intended title/axes and a
large diagonal "PROVISIONAL" watermark plus a note describing what the final figure
will show. When the campaign data lands, build_figures.sh overwrites the file with the
real plot (CSV-driven), and the figure float in the thesis needs no change.

  experiments/mitsuba-reference/.venv/bin/python scripts/plots/placeholder.py \
      --title "RIS vs MIS, equal quality" --note "K-sweep; env-map vs flat" \
      --out latex/figures/ris_ksweep.pdf
"""
import argparse
import os

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
STYLE = os.path.join(HERE, "style.mplstyle")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--title", required=True)
    ap.add_argument("--note", default="")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    if os.path.exists(STYLE):
        plt.style.use(STYLE)
    fig, ax = plt.subplots()
    ax.set_title(args.title)
    ax.set_xticks([])
    ax.set_yticks([])
    if args.note:
        ax.text(0.5, 0.62, args.note, ha="center", va="center", style="italic",
                fontsize=11, transform=ax.transAxes)
    ax.text(0.5, 0.5, "PROVISIONAL", ha="center", va="center", rotation=20,
            fontsize=40, color="0.85", fontweight="bold", transform=ax.transAxes,
            zorder=0)
    ax.text(0.5, 0.36, "pending full-blast campaign", ha="center", va="center",
            fontsize=9, color="0.5", transform=ax.transAxes)
    for s in ax.spines.values():
        s.set_visible(True)

    out_dir = os.path.dirname(os.path.abspath(args.out))
    os.makedirs(out_dir, exist_ok=True)
    fig.savefig(args.out)
    print(f"wrote placeholder {args.out}")


if __name__ == "__main__":
    main()
