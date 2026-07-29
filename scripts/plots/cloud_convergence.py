#!/usr/bin/env python3
"""Phase-1 cloud-meadow convergence analysis (advisor meeting 2026-06-25).

Reads results/campaign/cloud_conv/{ours,mits_analog,mits_nee}_spp{S}_seed{N}.exr + times.csv and produces:
  (A) disagreement test  : converged mean vs spp -- do NEE and analog meet? (they don't => one biased)
  (B) variance vs samples: per-pixel inter-seed variance vs spp, log-log (efficiency, NOT bias)
  (C) variance vs time   : variance vs wall-time; equal-variance speedup = time-ratio at fixed variance.
Variance is reported both raw and radiance-clipped (global 99.9th pct, the conservative thesis convention).

  experiments/mitsuba-reference/.venv/bin/python scripts/plots/cloud_convergence.py
"""
import glob, os, re, csv, statistics as st
from collections import defaultdict
import numpy as np, OpenEXR, Imath
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt

CONV = "results/campaign/cloud_conv"
OUTDIR = "thesis/latex/figures"
ARMS = ["ours", "mits_analog", "mits_nee"]
LABEL = {"ours": "ours (MIS)", "mits_analog": "Mitsuba analog", "mits_nee": "Mitsuba NEE"}
COLOR = {"ours": "#2e8b57", "mits_analog": "#30638e", "mits_nee": "#d1495b"}
SPP = 64  # spp for k = var*spp scaling (per-arm spp recorded in the filename)

def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()["dataWindow"]
    w = dw.max.x - dw.min.x + 1; h = dw.max.y - dw.min.y + 1
    pt = Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c, pt), np.float32).reshape(h, w) for c in ("R", "G", "B")], -1)

# stacks[arm][spp] = list of images
stacks = defaultdict(lambda: defaultdict(list))
for p in sorted(glob.glob(f"{CONV}/*.exr")):
    m = re.search(r"(ours|mits_analog|mits_nee)_spp(\d+)_seed(\d+)", os.path.basename(p))
    if m:
        stacks[m.group(1)][int(m.group(2))].append(p)

style = os.path.join(os.path.dirname(__file__), "style.mplstyle")
if os.path.exists(style):
    plt.style.use(style)

def stats(paths, clip_thresh):
    """mean, raw per-pixel inter-seed variance, and clipped variance using a FIXED per-arm radiance
    threshold (passed in) so the clipped curve is comparable across spp -- recomputing the percentile
    per cell makes clipped variance non-monotone in spp, which is meaningless."""
    A = np.array([load(p) for p in paths])
    mean = float(A.mean())
    var_raw = float(A.var(0, ddof=1).mean())
    var_clip = float(np.minimum(A, clip_thresh).var(0, ddof=1).mean())
    return mean, var_raw, var_clip, len(A)

# timing medians
tmed = defaultdict(dict)
if os.path.exists(f"{CONV}/times.csv"):
    rows = defaultdict(list)
    for r in csv.DictReader(open(f"{CONV}/times.csv")):
        if r["time_s"] not in ("", "NA"):
            rows[(r["arm"], int(r["spp"]))].append(float(r["time_s"]))
    for k, v in rows.items():
        tmed[k[0]][k[1]] = st.median(v)

# aggregate. Per-arm clip threshold = 99.9th radiance pct of that arm's finest-spp stack (fixed across
# spp so the clipped-variance curve is comparable). Raw variance is the unambiguous primary metric.
data = defaultdict(dict)  # data[arm][spp] = (mean,var_raw,var_clip,n)
spps_all = set()
clip_thresh = {}
for arm in ARMS:
    arm_spps = sorted(stacks[arm].keys())
    if not arm_spps:
        continue
    Amax = np.array([load(p) for p in stacks[arm][arm_spps[-1]]])
    clip_thresh[arm] = float(np.percentile(Amax, 99.9))
    for spp in arm_spps:
        data[arm][spp] = stats(stacks[arm][spp], clip_thresh[arm]); spps_all.add(spp)
spps = sorted(spps_all)
print("clip thresholds (99.9pct, per arm): " + ", ".join(f"{a}={clip_thresh.get(a,0):.3f}" for a in ARMS))

print(f"\n{'arm':14s}{'spp':>6s}{'mean':>9s}{'var_raw':>11s}{'var_clip':>11s}{'t_med(s)':>10s}{'n':>4s}")
for arm in ARMS:
    for spp in spps:
        if spp in data[arm]:
            mn, vr, vc, n = data[arm][spp]; t = tmed.get(arm, {}).get(spp, float('nan'))
            print(f"{arm:14s}{spp:6d}{mn:9.4f}{vr:11.3e}{vc:11.3e}{t:10.2f}{n:4d}")

# ---- Plot A: disagreement (mean vs spp) ----
fig, ax = plt.subplots(figsize=(5.5, 3.4))
for arm in ARMS:
    xs = [s for s in spps if s in data[arm]]; ys = [data[arm][s][0] for s in xs]
    ax.plot(xs, ys, "o-", color=COLOR[arm], label=LABEL[arm])
ax.set_xscale("log", base=2); ax.set_xticks(spps)
ax.get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter())
ax.set_xlabel("samples per pixel"); ax.set_ylabel("converged image mean")
ax.set_title("Cloud--meadow: NEE vs analog do not converge to a common mean")
ax.legend(fontsize=8); fig.tight_layout(); fig.savefig(f"{OUTDIR}/cloud_mean_convergence.pdf")
print(f"wrote {OUTDIR}/cloud_mean_convergence.pdf")

# ---- Plot B+C: variance vs samples, variance vs time (ours vs analog, the unbiased pair) ----
# RAW per-pixel variance: clean and monotone (~1/spp). The 99.9pct firefly-clip is NOT used for the
# curves -- a fixed radiance threshold can't be spp-invariant (fireflies dilute with spp), so clipped
# variance is non-monotone; the clip belongs to the single-point headline number, not a convergence axis.
VR = 1  # index of var_raw
fig, (axS, axT) = plt.subplots(1, 2, figsize=(11, 3.4))
for arm in ("ours", "mits_analog"):
    xs = [s for s in spps if s in data[arm]]
    vr = [data[arm][s][VR] for s in xs]
    axS.plot(xs, vr, "o-", color=COLOR[arm], label=LABEL[arm])
    ts = [tmed.get(arm, {}).get(s, float('nan')) for s in xs]
    axT.plot(ts, vr, "o-", color=COLOR[arm], label=LABEL[arm])
for ax in (axS, axT):
    ax.set_xscale("log"); ax.set_yscale("log"); ax.set_ylabel("per-pixel variance (raw)")
    ax.legend(fontsize=8)
axS.set_xlabel("samples per pixel"); axS.set_xticks(spps)
axS.get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter())
axT.set_xlabel("wall-clock render time (s)")
axS.set_title("Variance vs samples"); axT.set_title("Variance vs time (equal-variance = horizontal gap)")
fig.tight_layout(); fig.savefig(f"{OUTDIR}/cloud_equalvar.pdf")
print(f"wrote {OUTDIR}/cloud_equalvar.pdf")

# ---- equal-variance speedup: time to reach a common variance (variance ~ C/time) ----
def interp_time(arm, idx, target_v):
    xs = [s for s in spps if s in data[arm]]
    v = np.array([data[arm][s][idx] for s in xs]); t = np.array([tmed.get(arm, {}).get(s, np.nan) for s in xs])
    i = int(np.argmin(v)); return t[i] * v[i] / target_v   # t to reach target_v from the finest point
for idx, name in ((1, "raw"), (2, "clipped")):
    common_v = min(data["ours"][max(spps)][idx], data["mits_analog"][max(spps)][idx])
    to = interp_time("ours", idx, common_v); ta = interp_time("mits_analog", idx, common_v)
    print(f"Equal-variance ({name}) at V*={common_v:.3e}: ours {to:.0f}s vs analog {ta:.0f}s -> {ta/to:.0f}x")
print("Note: raw is firefly-dominated (analog's bright-sun escapes); the 99.9pct-clip number is the "
      "conservative, firefly-robust equal-quality figure consistent with the thesis headline.")
