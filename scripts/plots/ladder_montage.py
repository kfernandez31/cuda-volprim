#!/usr/bin/env python3
"""Validation-ladder montage: rows = complexity rungs, cols = ours | reference | |diff|.

Builds either the absorption ladder (single Gaussian | overlapping pair | cloud; reference =
closed-form analytic for the single, Mitsuba-volprim for pair+cloud) or the scattering ladder
(single | cluster | cloud; reference = Mitsuba-analog). Each rung is tonemapped independently;
the difference column is amplified and annotated with the converged-mean ratio + RMSE.

  tools/refs/.venv/bin/python scripts/plots/ladder_montage.py absorption  thesis/latex/figures/absorption_ladder.pdf
  tools/refs/.venv/bin/python scripts/plots/ladder_montage.py scattering  thesis/latex/figures/scattering_ladder.pdf
"""
import sys, os, math
import numpy as np, OpenEXR, Imath
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt

LAD = "results/campaign/ladder"


def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()["dataWindow"]
    w = dw.max.x - dw.min.x + 1; h = dw.max.y - dw.min.y + 1
    pt = Imath.PixelType(Imath.PixelType.FLOAT); ch = f.header()["channels"].keys()
    return np.stack([np.frombuffer(f.channel(c, pt), np.float32).reshape(h, w)
                     for c in ("R", "G", "B") if c in ch], -1)


def analytic_single(M=1.0, width=256, height=256, extent=6.0, env=1.0):
    """Closed-form single isotropic Gaussian, ortho view: I = exp(-tau)*env, tau=M/2pi exp(-d^2/2)."""
    js = (np.arange(width) + 0.5) / width * extent - 0.5 * extent
    is_ = (np.arange(height) + 0.5) / height * extent - 0.5 * extent
    PX, PY = np.meshgrid(js, is_)
    d2 = PX * PX + PY * PY
    tau = (M / (2 * math.pi)) * np.exp(-0.5 * d2)
    return np.repeat((np.exp(-tau) * env)[..., None], 3, -1).astype(np.float32)


def tm(a, e):
    return np.clip(a * e, 0, 1) ** (1 / 2.2)


# rung spec: (label, ours_file, ref_file_or_None_for_analytic, exposure)
LADDERS = {
    "absorption": [
        ("single Gaussian\n(vs analytic)", "abs_single_ours.exr", None, 1.0),
        ("overlapping pair\n(vs Mitsuba)", "abs_pair_ours.exr", "abs_pair_ref.exr", 1.0),
        ("full cloud\n(vs Mitsuba)", "abs_cloud_ours.exr", "abs_cloud_ref.exr", 1.0),
    ],
    "scattering": [
        ("single Gaussian", "sc_single_ours.exr", "sc_single_ref.exr", 2.2),
        ("cluster", "sc_cluster_ours.exr", "sc_cluster_ref.exr", 2.2),
        ("full cloud", "sc_cloud_ours.exr", "sc_cloud_ref.exr", 1.3),
    ],
}
AMP = 5.0


def main():
    which, out = sys.argv[1], sys.argv[2]
    rungs = LADDERS[which]
    n = len(rungs)
    fig, axes = plt.subplots(n, 3, figsize=(8.4, 2.7 * n))
    if which == "scattering":
        ref_label = "Mitsuba-analog"
    else:
        ref_label = "reference"
    for r, (label, of, rf, e) in enumerate(rungs):
        ours = load(os.path.join(LAD, of))
        ref = analytic_single() if rf is None else load(os.path.join(LAD, rf))
        # match shapes (defensive)
        if ref.shape != ours.shape:
            ref = ref[:ours.shape[0], :ours.shape[1]]
        rmse = float(np.sqrt(((ours - ref) ** 2).mean()))
        ratio = float(ours.mean() / ref.mean())
        diff = np.clip(np.abs(ours - ref).mean(-1) * AMP, 0, 1)
        thisref = "analytic" if rf is None else ref_label
        col_titles = ["ours", "reference", f"$|\\Delta|\\times{AMP:.0f}$"]
        for c, (img, cmap) in enumerate([
                (tm(ours, e), None),
                (tm(ref, e), None),
                (diff, "magma")]):
            ax = axes[r, c]
            ax.imshow(img, cmap=cmap); ax.axis("off")
            if r == 0:
                ax.set_title(col_titles[c], fontsize=10)
        axes[r, 0].set_ylabel(label, fontsize=9, rotation=90, labelpad=8)
        axes[r, 0].axis("on"); axes[r, 0].set_xticks([]); axes[r, 0].set_yticks([])
        for sp in axes[r, 0].spines.values():
            sp.set_visible(False)
        axes[r, 2].text(0.5, -0.04, f"ratio {ratio:.4f},  RMSE {rmse:.4f}",
                        transform=axes[r, 2].transAxes, ha="center", va="top", fontsize=7.5)
        print(f"{which} {label.splitlines()[0]:18s} ratio={ratio:.4f} RMSE={rmse:.4f}")
    plt.subplots_adjust(wspace=0.04, hspace=0.18, left=0.05, right=0.99, top=0.95, bottom=0.03)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    fig.savefig(out, dpi=150, bbox_inches="tight")
    fig.savefig(os.path.splitext(out)[0] + ".png", dpi=150, bbox_inches="tight")
    print("wrote", out)


if __name__ == "__main__":
    main()
