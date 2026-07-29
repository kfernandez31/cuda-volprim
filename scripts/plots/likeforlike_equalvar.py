#!/usr/bin/env python3
"""Headline figure: clipped per-pixel variance vs spp and vs render time,
this renderer (MIS) vs reference (corrected NEE). Locked-clock per-sample times from
results/campaign/nee_fair/timing (120.4 / 417.2 ms per spp).
Usage: likeforlike_equalvar.py --out latex/figures/likeforlike_equalvar.pdf
"""
import argparse, glob, os
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import mitsuba as mi
mi.set_variant("scalar_rgb")

HERE = os.path.dirname(os.path.abspath(__file__))
STYLE = os.path.join(HERE, "style.mplstyle")
LUM = np.array([0.2126, 0.7152, 0.0722])
T_OURS, T_NEE = 0.120414, 0.417232       # s per spp @ locked clocks

def seeds(patterns):
    fs = []
    for p in patterns:
        fs += sorted(glob.glob(p))
    assert fs, patterns
    return np.stack([np.array(mi.Bitmap(f)).astype(np.float64)[..., :3] @ LUM for f in fs])

OURS = {16: ["results/campaign/nee_fair/ladder/ours_spp16_seed*.exr"],
        64: ["results/campaign/g1_seeds/cuda_seed*.exr"],
        256: ["results/campaign/cloud_conv/ours_spp256_seed*.exr"],
        1024: ["results/campaign/cloud_conv/ours_spp1024_seed*.exr"],
        2048: ["results/campaign/cloud_conv/ours_spp2048_seed*.exr"],
        4096: ["results/campaign/cloud_conv/ours_spp4096_seed*.exr"]}
NEE = {s: [f"results/campaign/nee_fair/ladder_fix5/gabor_nee_meadow_spp{s}_seed*.exr"]
       for s in (16, 64, 256, 1024, 2048, 4096)}

def series(spec):
    ref = seeds(spec[64])
    clip = np.percentile(ref, 99.9)
    out = {}
    for spp, pats in spec.items():
        st = np.clip(seeds(pats), 0, clip)
        out[spp] = st.var(0, ddof=1).mean()
    return out

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--out", required=True)
    a = ap.parse_args()
    vo, vn = series(OURS), series(NEE)
    if os.path.exists(STYLE):
        plt.style.use(STYLE)
    fig, axes = plt.subplots(1, 2, figsize=(9.0, 3.3))
    arms = [("this renderer (MIS)", vo, T_OURS, "#1f77b4", "o"),
            ("reference (corrected NEE)", vn, T_NEE, "#ff7f0e", "s")]
    for label, v, tps, col, mk in arms:
        spp = np.array(sorted(v)); var = np.array([v[s] for s in spp])
        axes[0].loglog(spp, var, mk + "-", color=col, ms=4, lw=1.3, label=label)
        axes[1].loglog(spp * tps, var, mk + "-", color=col, ms=4, lw=1.3, label=label)
    g = np.array([16, 4096])
    v0 = vo[16]
    axes[0].loglog(g, v0 * (16 / g), "k--", lw=1, alpha=0.5, label=r"$1/\mathrm{spp}$")
    axes[0].set_xlabel("samples per pixel")
    axes[1].set_xlabel("render time (s), locked clocks")
    axes[0].set_ylabel("clipped per-pixel variance")
    # annotate the equal-quality gap on the time panel
    vt = vn[256]
    t_n = 256 * T_NEE
    t_o = t_n / 2.72
    axes[1].annotate("", xy=(t_o, vt), xytext=(t_n, vt),
                     arrowprops=dict(arrowstyle="<->", color="k", lw=1))
    axes[1].text(np.sqrt(t_o * t_n), vt * 1.25, r"$2.72\times$", ha="center", fontsize=9)
    for ax in axes:
        ax.grid(True, which="both", alpha=0.25)
    axes[0].legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(a.out); fig.savefig(os.path.splitext(a.out)[0] + ".png", dpi=140)
    print("wrote", a.out, {k: round(float(x), 6) for k, x in vo.items()}, {k: round(float(x), 6) for k, x in vn.items()})

if __name__ == "__main__":
    main()
