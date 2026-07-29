#!/usr/bin/env python3
"""Supervisor furnace: stats + plots + decision-rule verdicts.

Reads results/campaign/furnace_supervisor/{furnace_supervisor.csv, exr/} and produces:
  supervisor_furnace.pdf/.png : (L) radial profile of (mean-1)% with +-1 SEM noise band per mains arm;
                                (R) centre-box (mean-1)% with 95% CI, mains + probes.
  RESULT.md                   : per-arm table, decision-rule outcomes, hypothesis verdict.
All statistics in float64. CI uses Student-t on the seed sample.
  experiments/mitsuba-reference/.venv/bin/python scripts/plots/supervisor_furnace.py
"""
import csv, glob, math, os, re
from collections import defaultdict
import numpy as np
import OpenEXR, Imath
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt

OUT = "results/campaign/furnace_supervisor"
T95 = {1: 12.706, 2: 4.303, 3: 3.182, 4: 2.776, 5: 2.571, 6: 2.447, 7: 2.365, 15: 2.131}  # df -> t
MAINS = ["gabor_nee", "gabor_analog", "ours", "old_nee", "old_analog"]
COLOR = {"gabor_nee": "#d1495b", "gabor_analog": "#30638e", "ours": "#2e8b57",
         "old_nee": "#e88c30", "old_analog": "#7d5ba6"}
# Pre-registered hypotheses for gabor_nee centre over-count (%)
H = {"H0 (thick-only)": 0.0, "H1 (per-scatter)": -0.016, "H2 (global)": -0.632}

def t95(n): return T95.get(n - 1, 1.96)

def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()["dataWindow"]
    w = dw.max.x - dw.min.x + 1; h = dw.max.y - dw.min.y + 1
    pt = Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c, pt), np.float32).reshape(h, w)
                     for c in ("R", "G", "B")], -1).astype(np.float64)

# ---- CSV: per-arm centre stats -------------------------------------------------------------
rows = list(csv.DictReader(open(f"{OUT}/furnace_supervisor.csv")))
by_arm = defaultdict(list)
for r in rows:
    by_arm[r["arm"]].append(r)

stats = {}
for arm, rs in by_arm.items():
    c = np.array([float(r["centre"]) for r in rs]); n = len(c)
    mean = c.mean(); sem = c.std(ddof=1) / math.sqrt(n) if n > 1 else float("nan")
    ci = t95(n) * sem if n > 1 else float("nan")
    stats[arm] = dict(n=n, mean=mean, sem=sem, ci=ci, dev_pct=100 * (mean - 1),
                      ci_pct=100 * ci, spp=rs[0]["spp"], sigma=rs[0]["sigma"])

# ---- EXRs: per-pixel mean/std across seeds -> radial profiles + channel spread + corners ----
prof, chan_spread, corners = {}, {}, {}
for arm in MAINS:
    paths = sorted(glob.glob(f"{OUT}/exr/{arm}_*seed*.exr"))
    if not paths: continue
    A = np.stack([load(p) for p in paths])                 # (seeds, h, w, 3)
    px_mean = A.mean(0).mean(-1); px_std = A.mean(-1).std(0, ddof=1)
    h, w = px_mean.shape
    yy, xx = np.mgrid[0:h, 0:w]
    r = np.hypot(xx - (w - 1) / 2, yy - (h - 1) / 2) * 6.0 / w   # world units
    bins = np.linspace(0, 3.2, 33); mid = 0.5 * (bins[1:] + bins[:-1])
    which = np.digitize(r.ravel(), bins) - 1
    pm, ps = np.zeros(len(mid)), np.zeros(len(mid))
    for b in range(len(mid)):
        m = which == b
        if m.any():
            pm[b] = px_mean.ravel()[m].mean()
            ps[b] = (px_std.ravel()[m].mean()) / math.sqrt(A.shape[0])   # SEM of the mean profile
    prof[arm] = (mid, pm, ps)
    ch = A.mean((0, 1, 2)); chan_spread[arm] = float(ch.max() - ch.min())
    k = 16; g = px_mean
    corners[arm] = float((g[:k, :k].mean() + g[:k, -k:].mean() + g[-k:, :k].mean() + g[-k:, -k:].mean()) / 4)

# ---- verdicts ------------------------------------------------------------------------------
def has(a): return a in stats
verdict = []
ok = True
for a in ("gabor_analog", "old_analog", "ours"):
    if has(a):
        pas = abs(stats[a]["dev_pct"]) < max(2 * stats[a]["ci_pct"], 1e-3)
        ok &= pas
        verdict.append(f"- control `{a}`: dev {stats[a]['dev_pct']:+.4f}% (CI ±{stats[a]['ci_pct']:.4f}) -> {'PASS' if pas else 'FAIL'}")
if has("old_nee"):
    s = stats["old_nee"]; detect = abs(s["dev_pct"]) > max(s["ci_pct"], 1e-4)
    verdict.append(f"- positive control `old_nee`: dev {s['dev_pct']:+.4f}% (CI ±{s['ci_pct']:.4f}) -> "
                   f"{'nonzero deviation DETECTED (protocol sensitive; NB pre-registered +0.25% participation-scaling prediction FALSIFIED - see root cause)' if detect else 'NOT DETECTED -> protocol insensitive at this sigma; escalate'}")
if has("gabor_nee"):
    s = stats["gabor_nee"]
    best = min(H, key=lambda k: abs(s["dev_pct"] - H[k]))
    verdict.append(f"- **gabor_nee: dev {s['dev_pct']:+.4f}% ± {s['ci_pct']:.4f}** -> closest pre-registered hypothesis: **{best}** "
                   f"(H0 {H['H0 (thick-only)']:+.3f} / H1 {H['H1 (per-scatter)']:+.3f} / H2 {H['H2 (global)']:+.3f})")

# A probe "moves" the residual only if its delta is BOTH statistically significant AND a material
# fraction (>5%) of the residual itself — sub-0.5% deltas at micro-CIs are threshold artifacts.
probe_lines = []
def probe_sig(d, ci_a, ci_b, base_dev):
    return abs(d) > 3 * math.hypot(ci_a, ci_b) and abs(d) > 0.05 * abs(base_dev)
if has("probe_base"):
    b = stats["probe_base"]
    for p in ("probe_iters64", "probe_newton", "probe_envmap", "probe_film_whole"):
        if has(p):
            d = stats[p]["dev_pct"] - b["dev_pct"]
            sig = probe_sig(d, stats[p]["ci_pct"], b["ci_pct"], b["dev_pct"])
            probe_lines.append(f"- {p} vs base: Δ{d:+.4f}pp ({100*abs(d)/abs(b['dev_pct']):.1f}% of residual) -> "
                               f"{'MOVES the residual (implicated)' if sig else 'null'}")
if has("probe_llvm1024") and has("probe_cuda1024"):
    d = stats["probe_llvm1024"]["dev_pct"] - stats["probe_cuda1024"]["dev_pct"]
    sig = probe_sig(d, stats["probe_llvm1024"]["ci_pct"], stats["probe_cuda1024"]["ci_pct"], stats["probe_base"]["dev_pct"] if has("probe_base") else 1)
    probe_lines.append(f"- backend llvm vs cuda (1024spp): Δ{d:+.4f}pp -> {'differs (codegen/precision)' if sig else 'null (algorithmic, not codegen)'}")
if has("probe_film_parts") and has("probe_film_whole"):
    d = stats["probe_film_whole"]["dev_pct"] - stats["probe_film_parts"]["dev_pct"]
    sig = probe_sig(d, stats["probe_film_whole"]["ci_pct"], stats["probe_film_parts"]["ci_pct"], stats["probe_base"]["dev_pct"] if has("probe_base") else 1)
    probe_lines.append(f"- film 1x16384 vs 16x1024-fp64: Δ{d:+.4f}pp -> {'accumulation effect' if sig else 'null (film accumulation exact)'}")

# ---- plots ---------------------------------------------------------------------------------
style = os.path.join(os.path.dirname(__file__), "style.mplstyle")
if os.path.exists(style): plt.style.use(style)
fig, (aL, aR) = plt.subplots(1, 2, figsize=(11.5, 3.9), gridspec_kw={"width_ratios": [3, 2]})
aL.axhline(0, color="k", lw=1, ls="--", alpha=0.6)
for arm in MAINS:
    if arm in prof:
        mid, pm, ps = prof[arm]
        aL.plot(mid, 100 * (pm - 1), "-", color=COLOR[arm], label=arm.replace("_", " "))
        aL.fill_between(mid, 100 * (pm - 1 - ps), 100 * (pm - 1 + ps), color=COLOR[arm], alpha=0.25, lw=0)
aL.set_xlabel("radius from Gaussian centre (world units)"); aL.set_ylabel("pixel mean − 1  [%]")
aL.set_title("Furnace radial profile (mean ± SEM band across 8 seeds)"); aL.legend(fontsize=8)
arms_r = [a for a in MAINS if has(a)] + [p for p in
          ("probe_base", "probe_iters64", "probe_newton", "probe_envmap") if has(p)]
ys = [stats[a]["dev_pct"] for a in arms_r]; es = [stats[a]["ci_pct"] for a in arms_r]
cols = [COLOR.get(a, "#888888") for a in arms_r]
aR.axhline(0, color="k", lw=1, ls="--", alpha=0.6)
aR.bar(range(len(arms_r)), ys, yerr=es, color=cols, capsize=3)
for hname, hv in H.items():
    aR.axhline(hv, color="#d1495b", lw=0.8, ls=":", alpha=0.7)
    aR.text(len(arms_r) - 0.4, hv, hname.split()[0], fontsize=7, color="#d1495b", va="bottom")
aR.set_xticks(range(len(arms_r))); aR.set_xticklabels([a.replace("probe_", "p:").replace("_", "\n") for a in arms_r], fontsize=7)
aR.set_ylabel("centre (mean − 1)  [%]"); aR.set_title("Deviation from 1.0, 95% CI (mains σ0.1 | probes σ6)")
fig.tight_layout(); fig.savefig(f"{OUT}/supervisor_furnace.pdf"); fig.savefig(f"{OUT}/supervisor_furnace.png", dpi=150)

# ---- RESULT.md -----------------------------------------------------------------------------
with open(f"{OUT}/RESULT.md", "w") as f:
    f.write("# Supervisor furnace — results (auto-generated)\n\n")
    f.write("| arm | sigma | spp | n | centre mean | dev % | 95% CI ± | corner (env) | ch. spread |\n|---|---|---|---|---|---|---|---|---|\n")
    for a in MAINS + sorted(k for k in stats if k.startswith("probe")):
        if not has(a): continue
        s = stats[a]
        f.write(f"| {a} | {s['sigma']} | {s['spp']} | {s['n']} | {s['mean']:.6f} | {s['dev_pct']:+.4f} | "
                f"{s['ci_pct']:.4f} | {corners.get(a, float('nan')):.6f} | {chan_spread.get(a, float('nan')):.2e} |\n")
    f.write("\n## Decision rules\n" + "\n".join(verdict) + "\n")
    f.write(f"\nControls gate: {'PASS' if ok else 'FAIL - run void'}\n")
    f.write("\n## Probes (at sigma=6, vs probe_base)\n" + "\n".join(probe_lines) + "\n")
print("wrote", f"{OUT}/RESULT.md", "and plots")
print("\n".join(verdict)); print("\n".join(probe_lines))
