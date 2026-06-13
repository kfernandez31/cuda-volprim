#!/usr/bin/env python3
"""k-extraction for equal-quality campaigns (thesis k-convention: k = noise^2 * N).

Reads <dir>/<arm>_s<seed>.exr for each arm and seed, computes per-pixel inter-seed
variance (ddof=1), averages over pixels+channels, multiplies by spp -> k per arm.
With --times (CSV: arm,seed,time_s) also computes per-block-normalized median
relative time, eff = k * t_rel, and bootstrap CIs on eff_base/eff_arm.

Usage:
  tools/refs/.venv/bin/python scripts/tools/extract_k.py --dir results/campaign/rr_seeds \
      --arms d5 d6 d8 d10 d12 d16 --seeds 1 16 --spp 64 [--times .../times.csv --base d12]
"""
import argparse, csv, statistics as st
import numpy as np, OpenEXR, Imath

def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()['dataWindow']
    w = dw.max.x - dw.min.x + 1; h = dw.max.y - dw.min.y + 1
    pt = Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c, pt), dtype=np.float32).reshape(h, w)
                     for c in ('R', 'G', 'B')], -1)

ap = argparse.ArgumentParser()
ap.add_argument('--dir', required=True)
ap.add_argument('--arms', nargs='+', required=True)
ap.add_argument('--seeds', nargs=2, type=int, default=[1, 16])
ap.add_argument('--spp', type=int, required=True)
ap.add_argument('--times')          # optional CSV: arm,seed,time_s
ap.add_argument('--base')           # arm name for speedup denominator
ap.add_argument('--boot', type=int, default=50)
a = ap.parse_args()
seeds = list(range(a.seeds[0], a.seeds[1] + 1))

stacks = {arm: np.stack([load(f'{a.dir}/{arm}_s{s}.exr') for s in seeds]) for arm in a.arms}
k = {arm: float(stacks[arm].var(axis=0, ddof=1).mean()) * a.spp for arm in a.arms}

tmed, trel = {}, {}
if a.times:
    rows = [(r[0], int(r[1]), float(r[2])) for r in list(csv.reader(open(a.times)))[1:]]
    blocks = {}
    for arm, s, t in rows: blocks.setdefault(s, {})[arm] = t
    bm = {s: st.mean(b.values()) for s, b in blocks.items()}
    for arm in a.arms:
        tmed[arm] = st.median([blocks[s][arm] for s in seeds if arm in blocks.get(s, {})])
        trel[arm] = st.median([blocks[s][arm] / bm[s] for s in seeds if arm in blocks.get(s, {})])

print(f"{'arm':>8} {'k':>10}" + (f" {'t_med':>8} {'t_rel':>7} {'eff':>9}" if a.times else ""))
for arm in a.arms:
    line = f"{arm:>8} {k[arm]:>10.5f}"
    if a.times: line += f" {tmed[arm]:>8.3f} {trel[arm]:>7.4f} {k[arm]*trel[arm]:>9.5f}"
    print(line)

if a.times and a.base:
    rng = np.random.default_rng(0)
    print(f"\nspeedup vs {a.base} (eff ratio), bootstrap {a.boot} resamples:")
    for arm in a.arms:
        if arm == a.base: continue
        boots = []
        for _ in range(a.boot):
            idx = rng.integers(0, len(seeds), len(seeds))
            kb = float(stacks[a.base][idx].var(axis=0, ddof=1).mean()) * a.spp
            ka = float(stacks[arm][idx].var(axis=0, ddof=1).mean()) * a.spp
            tb = st.median([blocks[seeds[i]][a.base] for i in idx])
            ta = st.median([blocks[seeds[i]][arm] for i in idx])
            boots.append(kb * tb / (ka * ta))
        lo, hi = np.percentile(boots, [2.5, 97.5])
        kk = k[a.base] * tmed[a.base] / (k[arm] * tmed[arm])
        print(f"  {arm:>6}: {kk:.3f}  [{lo:.3f}, {hi:.3f}]")
