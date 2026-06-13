#!/usr/bin/env python3
"""Asset parity triptychs: ours | Mitsuba-analog | difference, per asset row.
Usage: asset_triptych.py <out.png> <asset1> [asset2 ...]
Expects results/campaign/g10_<asset>_{ours,mits}.exr. Flips 'ours' vertically to match
Mitsuba's orientation (the known asset_validation camera flip)."""
import sys, numpy as np, OpenEXR, Imath
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt

def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()['dataWindow']
    w = dw.max.x-dw.min.x+1; h = dw.max.y-dw.min.y+1; pt = Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c, pt), np.float32).reshape(h, w) for c in ('R','G','B')], -1)

def tonemap(img): return np.clip(img, 0, 1)**(1/2.2)

out, assets = sys.argv[1], sys.argv[2:]
DIFF_AMP = 10
fig, axes = plt.subplots(len(assets), 3, figsize=(9, 3.1*len(assets)))
if len(assets) == 1: axes = axes[None, :]
for i, a in enumerate(assets):
    ours = load(f'results/campaign/g10_{a}_ours.exr')[::-1]   # flip to match Mitsuba
    mits = load(f'results/campaign/g10_{a}_mits.exr')
    diff = np.clip(np.abs(ours-mits)*DIFF_AMP, 0, 1)
    ratio = ours.mean()/mits.mean()
    for ax, img, title in zip(axes[i], [tonemap(ours), tonemap(mits), diff],
                              [f'{a} — ours', f'{a} — Mitsuba-analog (ratio {ratio:.4f})',
                               f'|diff| ×{DIFF_AMP} (incl. MC noise, 256 spp)']):
        ax.imshow(img); ax.set_title(title, fontsize=8); ax.axis('off')
plt.tight_layout(); plt.savefig(out, dpi=130, bbox_inches='tight'); print('wrote', out)
