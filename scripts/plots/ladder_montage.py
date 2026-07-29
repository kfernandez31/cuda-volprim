#!/usr/bin/env python3
"""Validation-ladder montage: rows = complexity rungs, cols = ours | reference | |diff|.

Builds either the absorption ladder (single Gaussian | overlapping pair | cloud; reference =
closed-form analytic for the single, Mitsuba-volprim for pair+cloud) or the scattering ladder
(single | cluster | cloud; reference = Mitsuba-analog). Each rung is tonemapped independently;
the difference column is amplified and annotated with the converged-mean ratio + RMSE.

  experiments/mitsuba-reference/.venv/bin/python scripts/plots/ladder_montage.py absorption  latex/figures/absorption_ladder.pdf
  experiments/mitsuba-reference/.venv/bin/python scripts/plots/ladder_montage.py scattering  latex/figures/scattering_ladder.pdf
"""
import sys, os, math
import numpy as np, OpenEXR, Imath
from scipy.special import erf
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt

LAD = "results/campaign/ladder"


def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()["dataWindow"]
    w = dw.max.x - dw.min.x + 1; h = dw.max.y - dw.min.y + 1
    pt = Imath.PixelType(Imath.PixelType.FLOAT); ch = f.header()["channels"].keys()
    return np.stack([np.frombuffer(f.channel(c, pt), np.float32).reshape(h, w)
                     for c in ("R", "G", "B") if c in ch], -1)


def analytic_single(M=1.0, width=256, height=256, extent=6.0, env=1.0, clip_sigma=3.0):
    """Closed-form single isotropic Gaussian, ortho view: I = exp(-tau)*env.

    tau is the line integral of the unit Gaussian density TRUNCATED at the renderer's clip_sigma
    (3-sigma) BVH bound. The renderer zeroes density beyond 3 sigma, so the untruncated integral
    M/(2pi) exp(-d^2/2) over-counts the clipped tail and leaves a spurious outline ("ring") in the
    diff that grows toward the periphery. The truncated integral multiplies by erf(L/sqrt2), where
    L = sqrt(max(0, clip_sigma^2 - d^2)) is the chord half-length within the clip ball, matching the
    renderer exactly (single-Gaussian RMSE 7.2e-4 -> 2.0e-5)."""
    js = (np.arange(width) + 0.5) / width * extent - 0.5 * extent
    is_ = (np.arange(height) + 0.5) / height * extent - 0.5 * extent
    PX, PY = np.meshgrid(js, is_)
    d2 = PX * PX + PY * PY
    chord = np.sqrt(np.clip(clip_sigma * clip_sigma - d2, 0.0, None))
    tau = (M / (2 * math.pi)) * np.exp(-0.5 * d2) * erf(chord / math.sqrt(2.0))
    return np.repeat((np.exp(-tau) * env)[..., None], 3, -1).astype(np.float32)


def tm(a, e):
    return np.clip(a * e, 0, 1) ** (1 / 2.2)


def stretch(a, lo):
    """Display contrast stretch for transmittance panels: map [lo, 1] -> [0, 1] then sRGB gamma.
    Display-only -- the diff image, RMSE and ratio are all computed on the RAW values. The floor `lo`
    is shared between the ours and reference panels of a rung so they stay directly comparable; thin
    absorption (single Gaussian, pair) is otherwise a faint grey blob on white."""
    return np.clip((a - lo) / max(1e-6, 1.0 - lo), 0, 1) ** (1 / 2.2)


# Provenance of the banked ladder EXRs in results/campaign/ladder/ (re-bank if regenerating):
#   abs_single_ours.exr : SG_ALBEDO=0 build/bin/Release/test_runner --scene single_gaussian_validation
#                         --sigma-multiplier 1.0 --spp 1024   (M=1.0, matches analytic_single M=1.0;
#                         centre px exp(-1/2pi)=0.853, verified RMSE 2e-5 vs the 3sigma-truncated analytic)
#   abs_pair_ours.exr / abs_cloud_ours.exr : same binary, scenes two_gaussian_* / cloud_asset_absorption.
#   abs_pair_ref.exr  : Mitsuba volprim absorption ref, 16384 spp (render_two_gaussian_via_prb.py,
#                        with_jorge_mitsuba.sh / shipped stack, 2026-07-09).
#   abs_cloud_ref.exr : Mitsuba volprim absorption ref cam0, mean of 24 x 2048 spp seeds (49k effective;
#                        render_cloud_prb_absorption.py, shipped stack, 2026-07-10). abs_cloud_ours: 4096 spp.
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
AMP = 10.0  # legacy (unused by the signed convention)
DIFF_SCALE = 0.05  # +-5% full scale, thesis-wide comparison-plot convention


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
        # thesis-wide diff convention: signed relative difference, red = ours brighter,
        # blue = ours darker, white = agreement; symmetric clip at +-DIFF_SCALE
        rel = (ours.mean(-1) - ref.mean(-1)) / np.maximum(ref.mean(-1), 1e-3)
        t = np.clip(rel / DIFF_SCALE, -1, 1)
        pos, neg = np.clip(t, 0, 1), np.clip(-t, 0, 1)
        diff = np.stack([1 - 0.85 * neg, 1 - 0.85 * pos - 0.55 * neg, 1 - 0.85 * pos], -1)
        thisref = "analytic" if rf is None else ref_label
        col_titles = ["ours", "reference", f"signed rel. diff (\u00b1{DIFF_SCALE*100:.0f}%)"]
        # Absorption rungs are transmittance (background = 1): contrast-stretch for display so the
        # thin structure reads. Scattering rungs keep the exposure tonemap.
        if which == "absorption":
            lo = float(min(ours.min(), ref.min()))
            disp_ours, disp_ref = stretch(ours, lo), stretch(ref, lo)
        else:
            disp_ours, disp_ref = tm(ours, e), tm(ref, e)
        for c, (img, cmap) in enumerate([
                (disp_ours, None),
                (disp_ref, None),
                (diff, None)]):
            ax = axes[r, c]
            ax.imshow(img, cmap=cmap); ax.axis("off")
            if r == 0:
                ax.set_title(col_titles[c], fontsize=10)
        axes[r, 0].set_ylabel(label, fontsize=9, rotation=90, labelpad=8)
        axes[r, 0].axis("on"); axes[r, 0].set_xticks([]); axes[r, 0].set_yticks([])
        for sp in axes[r, 0].spines.values():
            sp.set_visible(False)
        if rmse >= 1e-3:
            rtxt = f"{rmse:.4f}"
        else:
            e = int(np.floor(np.log10(rmse)))
            rtxt = f"${rmse / 10**e:.1f}\\times10^{{{e}}}$"
        axes[r, 2].text(0.5, -0.04, f"ratio {ratio:.4f},  RMSE {rtxt}",
                        transform=axes[r, 2].transAxes, ha="center", va="top", fontsize=10)
        print(f"{which} {label.splitlines()[0]:18s} ratio={ratio:.4f} RMSE={rmse:.4f}")
    plt.subplots_adjust(wspace=0.04, hspace=0.18, left=0.05, right=0.99, top=0.95, bottom=0.03)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    fig.savefig(out, dpi=150, bbox_inches="tight")
    fig.savefig(os.path.splitext(out)[0] + ".png", dpi=150, bbox_inches="tight")
    print("wrote", out)


if __name__ == "__main__":
    main()
