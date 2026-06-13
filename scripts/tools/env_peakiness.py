#!/usr/bin/env python3
"""Peakiness metrics for equirect RADIANCE .hdr envs: max/mean luminance ratio and
the energy fraction held by the top 0.1% of solid angle. Both sin(theta)-weighted.
Usage: env_peakiness.py a.hdr b.hdr ... [--csv out.csv]"""
import argparse, sys
import numpy as np

def read_hdr(path):
    with open(path, 'rb') as f:
        if not f.readline().startswith(b'#?'): raise ValueError('not RADIANCE')
        while True:
            line = f.readline()
            if line in (b'\n', b'\r\n'): break
        dims = f.readline().split()                  # b'-Y' H b'+X' W
        h, w = int(dims[1]), int(dims[3])
        data = np.empty((h, w, 4), np.uint8)
        for y in range(h):
            head = f.read(4)
            if head[:2] == b'\x02\x02' and (head[2] << 8 | head[3]) == w:  # new-style RLE
                for c in range(4):
                    x = 0
                    while x < w:
                        n = f.read(1)[0]
                        if n > 128:                   # run
                            data[y, x:x + n - 128, c] = f.read(1)[0]; x += n - 128
                        else:                         # literal
                            data[y, x:x + n, c] = np.frombuffer(f.read(n), np.uint8); x += n
            else:                                     # flat scanline
                row = head + f.read(4 * w - 4)
                data[y] = np.frombuffer(row, np.uint8).reshape(w, 4)
    e = data[..., 3].astype(np.int32)
    scale = np.where(e == 0, 0.0, np.ldexp(1.0, e - 136))   # 2^(E-128-8)
    rgb = data[..., :3].astype(np.float64) * scale[..., None]
    return rgb

ap = argparse.ArgumentParser(); ap.add_argument('hdrs', nargs='+'); ap.add_argument('--csv')
a = ap.parse_args()
rows = []
for p in a.hdrs:
    rgb = read_hdr(p)
    lum = rgb @ [0.2126, 0.7152, 0.0722]
    h, w = lum.shape
    sa = np.sin((np.arange(h) + 0.5) / h * np.pi)[:, None] * np.ones((1, w))  # ∝ solid angle
    energy = (lum * sa).ravel(); saw = sa.ravel()
    mean = energy.sum() / saw.sum()
    peak = lum.max() / mean
    order = np.argsort(energy)[::-1]
    cut = np.searchsorted(np.cumsum(saw[order]), 0.001 * saw.sum())
    frac = energy[order][:cut + 1].sum() / energy.sum()
    rows.append((p.split('/')[-1], w, h, lum.max(), mean, peak, frac))
    print(f"{p}: {w}x{h}  max/mean = {peak:.4g}   top-0.1%-solid-angle energy = {100*frac:.1f}%")
if a.csv:
    import csv as _c
    with open(a.csv, 'w', newline='') as f:
        _c.writer(f).writerows([('env', 'w', 'h', 'max_lum', 'mean_lum', 'peak_ratio', 'top01pct_energy')] + rows)
