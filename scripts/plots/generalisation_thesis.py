#!/usr/bin/env python3
"""Fig: cross-renderer generalisation (tornado/explosion/bunny), thesis conventions, v2.
Columns: ours | reference | signed rel. diff (white-midpoint convention, +-5%).
Reference arm = volprim_prb with corrected NEE (KramarzVolprimFixes2026); both arms under the
meadow environment (SG_ENV_ROTY=90, the validated showcase orientation). Panels are cropped to
the asset bounding box (computed from the clean white-env reference renders, same cameras,
padded), so the assets fill the frame; the white-env job-4 renders remain as parity data and
their mean ratios are printed for the prose.
"""
import numpy as np, os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import mitsuba as mi
mi.set_variant("scalar_rgb")

HERE = os.path.dirname(os.path.abspath(__file__))
STYLE = os.path.join(HERE, "style.mplstyle")
SCALE = 0.05
PAD = 15
D = "results/campaign/asset_neefix"
ASSETS = [("tornado", 2.2), ("explosion", 2.2), ("bunny", 2.2)]

def load(p):
    return np.array(mi.Bitmap(p), dtype=np.float64)[..., :3]

def avg(paths):
    return np.mean([load(p) for p in paths], axis=0)

def tm(img, ex):
    return np.clip(img * ex, 0, 1) ** (1 / 2.2)

def signed(a, b):
    la, lb = a.mean(-1), b.mean(-1)
    rel = (la - lb) / np.maximum(lb, 1e-3)
    t = np.clip(rel / SCALE, -1, 1)
    pos, neg = np.clip(t, 0, 1), np.clip(-t, 0, 1)
    return np.stack([1 - 0.85 * neg, 1 - 0.85 * pos - 0.55 * neg, 1 - 0.85 * pos], -1)

def bbox_from_white(name, shape):
    """Asset bounding box from the clean white-env reference render (bg == 1), padded."""
    w = avg([f"{D}/{name}_neefix_diag_spp1024_seed{s}.exr" for s in (0, 1)])
    mask = np.abs(w.mean(-1) - 1.0) > 0.02
    ys, xs = np.where(mask)
    y0, y1 = max(0, ys.min() - PAD), min(shape[0], ys.max() + PAD)
    x0, x1 = max(0, xs.min() - PAD), min(shape[1], xs.max() + PAD)
    return y0, y1, x0, x1

if os.path.exists(STYLE):
    plt.style.use(STYLE)
fig, axs = plt.subplots(3, 3, figsize=(8.6, 8.6))
for i, (name, ex) in enumerate(ASSETS):
    # asset_validation camera is vertically flipped vs Mitsuba (results/campaign/asset_parity.md);
    # align ours for display and the signed diff (the mean ratio is flip-invariant either way)
    import glob as _g
    _ours_files = sorted(_g.glob(f"{D}/{name}_ours_meadow_spp256_seed*.exr"))
    o = np.flipud(avg(_ours_files))
    print(f"{name}: ours = {len(_ours_files)} x 256 spp")
    m = avg([f"{D}/{name}_neefix_meadow_diag_spp1024_seed{s}.exr" for s in (0, 1, 2, 3)])
    if o.shape != m.shape:
        m = m[:o.shape[0], :o.shape[1]]
    ratio = o.mean() / m.mean()
    # white-env parity (job-4 renders, same cameras) for the prose
    ow = np.flipud(avg([f"{D}/{name}_ours_spp256_seed{s}.exr" for s in (1, 2, 3, 4)]))
    mw = avg([f"{D}/{name}_neefix_diag_spp1024_seed{s}.exr" for s in (0, 1)])
    ratio_w = ow.mean() / mw.mean()
    y0, y1, x0, x1 = bbox_from_white(name, o.shape)
    o, m = o[y0:y1, x0:x1], m[y0:y1, x0:x1]
    axs[i, 0].imshow(tm(o, ex)); axs[i, 0].set_ylabel(name, fontsize=10)
    axs[i, 1].imshow(tm(m, ex))
    axs[i, 2].imshow(signed(o, m))
    o_means = [load(p).mean() for p in _ours_files]
    m_means = [load(f"{D}/{name}_neefix_meadow_diag_spp1024_seed{s}.exr").mean() for s in (0, 1, 2, 3)]
    se = ratio * np.sqrt(np.var(o_means, ddof=1) / len(o_means) / np.mean(o_means) ** 2
                         + np.var(m_means, ddof=1) / len(m_means) / np.mean(m_means) ** 2)
    axs[i, 2].text(0.5, -0.08, f"ours/reference {ratio:.4f} $\\pm$ {se:.4f}",
                   transform=axs[i, 2].transAxes, ha="center", fontsize=10)
    print(f"{name}: meadow ratio {ratio:.5f} | white parity ratio {ratio_w:.5f} "
          f"| crop [{y0}:{y1},{x0}:{x1}]")
    if i == 0:
        axs[i, 0].set_title("ours", fontsize=10)
        axs[i, 1].set_title("reference", fontsize=10)
        axs[i, 2].set_title(f"signed rel. diff (±{SCALE*100:.0f}%)", fontsize=10)
    for j in range(3):
        axs[i, j].set_xticks([]); axs[i, j].set_yticks([])
        for sp in axs[i, j].spines.values():
            sp.set_visible(True); sp.set_edgecolor("#8a8a8a"); sp.set_linewidth(0.7)
fig.tight_layout()
fig.savefig("latex/figures/generalisation.pdf")
fig.savefig("latex/figures/generalisation.png", dpi=130)
print("wrote generalisation figures")
