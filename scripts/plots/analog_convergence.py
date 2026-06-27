#!/usr/bin/env python3
"""Sampler-only convergence: ours-ANALOG vs Mitsuba-ANALOG on the FLAT env.

Both renderers in analog mode (no NEE/MIS) -> isolates the SAMPLER. Flat env removes fireflies, so raw
per-pixel inter-seed variance is the clean, unambiguous noise metric (k_raw == k_clip). Produces:
  variance vs samples (log-log)  and  variance vs wall-time (log-log)
showing ours-analog is ~Nx noisier per sample but ~Mx faster per sample (net ~0.8x equal-variance).

  tools/refs/.venv/bin/python scripts/plots/analog_convergence.py
"""
import glob, os, re, csv, statistics as st
from collections import defaultdict
import numpy as np, OpenEXR, Imath
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt

CONV = "results/campaign/analog_conv"
OUT = "thesis/latex/figures/analog_convergence.pdf"
ARMS = ["ours_analog", "mits_analog"]
LABEL = {"ours_analog": "ours (analog)", "mits_analog": "Mitsuba (analog)"}
COLOR = {"ours_analog": "#2e8b57", "mits_analog": "#30638e"}

def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()["dataWindow"]
    w = dw.max.x - dw.min.x + 1; h = dw.max.y - dw.min.y + 1
    pt = Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c, pt), np.float32).reshape(h, w) for c in ("R", "G", "B")], -1)

style = os.path.join(os.path.dirname(__file__), "style.mplstyle")
if os.path.exists(style): plt.style.use(style)

stacks = defaultdict(lambda: defaultdict(list))
for p in sorted(glob.glob(f"{CONV}/*.exr")):
    m = re.search(r"(ours_analog|mits_analog)_spp(\d+)_seed(\d+)", os.path.basename(p))
    if m: stacks[m.group(1)][int(m.group(2))].append(p)

tmed = defaultdict(dict)
if os.path.exists(f"{CONV}/times.csv"):
    rows = defaultdict(list)
    for r in csv.DictReader(open(f"{CONV}/times.csv")):
        if r["time_s"] not in ("", "NA"): rows[(r["arm"], int(r["spp"]))].append(float(r["time_s"]))
    for k, v in rows.items(): tmed[k[0]][k[1]] = st.median(v)

data = defaultdict(dict)  # data[arm][spp] = (var, mean, n)
spps_all = set()
for arm in ARMS:
    for spp, paths in sorted(stacks[arm].items()):
        A = np.array([load(p) for p in paths])
        data[arm][spp] = (float(A.var(0, ddof=1).mean()), float(A.mean()), len(A)); spps_all.add(spp)
spps = sorted(spps_all)

print(f"{'arm':14s}{'spp':>6s}{'variance':>12s}{'mean':>9s}{'t_med':>9s}{'n':>4s}")
for arm in ARMS:
    for spp in spps:
        if spp in data[arm]:
            v, mn, n = data[arm][spp]; t = tmed.get(arm, {}).get(spp, float('nan'))
            print(f"{arm:14s}{spp:6d}{v:12.4e}{mn:9.4f}{t:9.2f}{n:4d}")

fig, (axS, axT) = plt.subplots(1, 2, figsize=(11, 3.6))
for arm in ARMS:
    xs = [s for s in spps if s in data[arm]]
    v = [data[arm][s][0] for s in xs]
    axS.plot(xs, v, "o-", color=COLOR[arm], label=LABEL[arm])
    ts = [tmed.get(arm, {}).get(s, float('nan')) for s in xs]
    axT.plot(ts, v, "o-", color=COLOR[arm], label=LABEL[arm])
for ax in (axS, axT):
    ax.set_xscale("log"); ax.set_yscale("log"); ax.set_ylabel("per-pixel variance"); ax.legend(fontsize=9)
axS.set_xlabel("samples per pixel"); axS.set_xticks(spps)
axS.get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter())
axT.set_xlabel("wall-clock render time (s)")
axS.set_title("Sampler only (flat env): variance vs samples")
axT.set_title("variance vs time (equal-variance = horizontal gap)")
fig.tight_layout(); fig.savefig(OUT); fig.savefig(os.path.splitext(OUT)[0] + ".png", dpi=150)
print("wrote", OUT)

# offsets at the finest common spp
fs = max(s for s in spps if s in data["ours_analog"] and s in data["mits_analog"])
vo, vm = data["ours_analog"][fs][0], data["mits_analog"][fs][0]
to, tm = tmed["ours_analog"].get(fs), tmed["mits_analog"].get(fs)
print(f"\nAt {fs} spp: ours var {vo:.3e} vs mits {vm:.3e} -> ours {vo/vm:.1f}x noisier per pixel")
if to and tm:
    print(f"  time: ours {to:.1f}s vs mits {tm:.1f}s -> ours {tm/to:.2f}x faster per sample")
    print(f"  net equal-variance (mits/ours of var*time) = {(vm*tm)/(vo*to):.2f}x  (>1 = ours wins)")
