#!/usr/bin/env python3
"""Cross-renderer agreement panels: meadow (peaky, ~5% dense-core diff) vs constant (uniform, ~1%).
2 rows x 3 cols: [ours-MIS | corrected-NEE | signed rel diff]. Visualises the Task-0 root cause AND
the scope-D constant-env validation in one figure. Output: agreement_panels.{pdf,png}.
"""
import glob, os
import numpy as np
import mitsuba as mi
mi.set_variant("scalar_rgb")
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

LUM = np.array([0.2126, 0.7152, 0.0722], np.float32); lum = lambda x: x @ LUM
OUT = "results/campaign/nee_fair"

def load_avg(p):
    fs = sorted(glob.glob(p)); assert fs, p
    return np.mean([np.array(mi.Bitmap(f)).astype(np.float64)[..., :3] for f in fs], 0)

def tonemap(rgb, ex):
    a = np.clip(lum(rgb) * ex, 0, 1)
    return np.stack([a, a, a], -1)

def signed_rel(a, b, norm=1.0, scale=0.20):
    rel = (lum(a) * norm - lum(b)) / np.maximum(lum(b), 1e-3)
    t = np.clip(rel / scale, -1, 1)
    pos, neg = np.clip(t, 0, 1), np.clip(-t, 0, 1)
    return np.stack([1 - 0.85 * neg, 1 - 0.85 * pos - 0.55 * neg, 1 - 0.85 * pos], -1)

def block_relrmse(a, b, blk=60, norm=1.0):
    la, lb = lum(a) * norm, lum(b); H, W = la.shape
    la = la.reshape(H//blk, blk, W//blk, blk).mean((1, 3))
    lb = lb.reshape(H//blk, blk, W//blk, blk).mean((1, 3))
    return 100 * np.sqrt(np.mean((la - lb)**2)) / lb.mean()

rows = []
# meadow
mo = load_avg("results/campaign/g1_seeds/cuda_seed*.exr")
mn = load_avg(f"{OUT}/gt/gabor_nee_meadow_spp2048_seed*.exr")
rows.append(("meadow (peaky sun)", mo, mn, 1.0))
# constant (normalize ours by background ratio so intensity offset doesn't masquerade as structure)
co = load_avg(f"{OUT}/constenv/ours_const_seed*.exr")
cn = load_avg(f"{OUT}/constenv/gabor_nee_white_constant_spp256_seed0.exr")
bo, bg = lum(co)[:60, :60].mean(), lum(cn)[:60, :60].mean()
rows.append(("constant (uniform)", co, cn, bg / bo))

fig, axs = plt.subplots(2, 3, figsize=(11, 6.0))
for i, (title, o, n, norm) in enumerate(rows):
    ex = 1.0 / np.percentile(lum(n), 99.5)
    rr = block_relrmse(o, n, 60, norm)
    ratio = (lum(o).mean() * norm) / lum(n).mean()
    axs[i, 0].imshow(tonemap(o, ex)); axs[i, 0].set_ylabel(title, fontsize=11)
    axs[i, 0].set_title("ours (MIS)" if i == 0 else "")
    axs[i, 1].imshow(tonemap(n, ex))
    axs[i, 1].set_title("corrected NEE" if i == 0 else "")
    axs[i, 2].imshow(signed_rel(o, n, norm))
    axs[i, 2].set_title("signed rel. diff (±20%)" if i == 0 else "")
    axs[i, 2].text(0.5, -0.09, f"coarse rel-RMSE {rr:.1f}%  ·  mean ratio {ratio:.4f}",
                   transform=axs[i, 2].transAxes, ha="center", fontsize=9)
    for j in range(3):
        axs[i, j].set_xticks([]); axs[i, j].set_yticks([])
fig.suptitle("Cross-renderer agreement: peaky vs uniform lighting "
             "(blue = ours darker, red = ours brighter)", fontsize=11)
fig.tight_layout(rect=[0, 0, 1, 0.96])
fig.savefig(f"{OUT}/agreement_panels.pdf"); fig.savefig(f"{OUT}/agreement_panels.png", dpi=130)
print(f"wrote {OUT}/agreement_panels.{{pdf,png}}")
print(f"  meadow @60 = {block_relrmse(mo,mn,60):.2f}%   constant @60 = {block_relrmse(co,cn,60,rows[1][3]):.2f}%")
