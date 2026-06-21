# G1 — headline (equal-quality vs Mitsuba) + handoff, 2026-06-13

Config: analytic baseline (NOT icosphere), MIS, calibrated caps, exact erf, 64 spp, 16 seeds, 350 W.
**No BARE / no "deficit" framing** (Kacper's call — Ch6/Ch8 already reframed, commit 4178be9).
Data (gitignored): `results/campaign/g1_seeds/` — `cuda_seed*` (ours-MIS), `mits_seed*` (Mitsuba-analog),
`ouranalog_seed*` (BROKEN, see A), `mitsnee_seed*` (Mitsuba-NEE), `bunny_seed*` (g1_bunny_seeds/).

## RESULT — cloud-meadow
| arm | mean | k | k_clip999 | t |
|---|---|---|---|---|
| ours-MIS (NEE on) | 0.3214 | **1.99** | 1.887 | ~9.0 s |
| Mitsuba-analog (NEE off) | 0.3201 | **3899** | 110.6 | ~9 s (wall 13.5 incl startup) |
| Mitsuba-NEE | 0.8199 | 3.05 | 2.99 | — |

### B (the discovery) — Mitsuba's NEE is energy-biased ✅ CLEAN
Anchored to the **analog GT** (Mitsuba-analog mean 0.3201, unbiased):
- **ours-NEE: +0.4 %** vs GT → CORRECT (within MC noise).
- **Mitsuba-NEE: +156.1 %** vs GT → massively energy-biased.
This is the honest, self-justifying framing: Mitsuba's NEE is broken, so its *only* unbiased mode is the
high-variance analog — not a hobble we imposed.

### Speed headline — ~59× clipped equal-quality (B-justified)
ours-MIS (k 1.99) vs Mitsuba-analog (k 3899) → **~59× clipped / ~2000× raw** less noise at ~equal render
time → equal-quality speedup ~59× (clipped; **95% bootstrap CI [54, 63]**, median 59, B=2000 over the 16
seeds, banked 2026-06-21; raw 1964× CI [1802, 2119]). Honest framing (no "hobbled Mitsuba"): *Mitsuba's NEE is
+156 % biased (B), so unbiased Mitsuba = analog (firefly-noisy); ours has correct MIS → ~59× faster.*
NOTE: t_mits is wall (incl startup); a clean steady-state Mitsuba render time would tighten the number
(k dominates, so ~59× holds).

### bunny rung (ours-internal) ✅
t_median **50.4 s** @64spp/512²/meadow, k=0.647, **0 overflows** (80/528 cap validated). No Mitsuba
(decided: bunny ours-internal; 3 native variants, matching ambiguous).

## A (core sampler: ours-analog vs Mitsuba-analog) — ❌ BLOCKED, root-caused
ours-analog rendered **17× too bright** (mean 5.54 vs 0.32). ROOT CAUSE: the renderer has **no true
analog mode**. `ENABLE_NEE=false` falls into the `else` branch at **`device/entry/raygen.cuh:270–276`**
= "unoccluded single-scatter" — `radiance += throughput*albedo*env(dir)` at every scatter, **no
occlusion/transmittance** → massive overcount in dense media. (Disabling `ENABLE_ANALYTIC_DIRECT` too
doesn't help — same 17×.) The *correct* analog estimator (env-on-escape, `raygen.cuh:187`) IS
implemented + validated (single-gaussian `H_analog`), but is combined with the unoccluded term.

**DISCOVERY DOES NOT WRECK ANY COMPLETED EXPERIMENT** — all reported runs used the default **NEE-on
MIS** (validated correct: G10 0.01–0.09 %, G1 +0.4 %, furnace). NEE-off was only touched in this
discarded A attempt (reverted; constants clean, build stock).

## CODE FIX (to enable A) — small
Add a **pure-analog** compile mode: when NEE off, **skip `raygen.cuh:271–276`** (the unoccluded
single-scatter) so env is gathered ONLY via the escape add (line 187). ~3 lines (new guard, e.g.
`ANALOG_PURE`, or `ENABLE_NEE=false && !unoccluded`). Validate: cloud-meadow mean → ~0.32, furnace PASS.
Then run A (ours-analog vs Mitsuba-analog → honest core free-flight-sampler speedup, clipped-k since
both firefly-noisy). Until then, G1 speed = the B-justified 59×.

---

# HANDOFF — how to proceed (2026-06-13, ~97% context)

**State:** branch `main` (campaign HEAD). Calibrated binaries in `~/winbins/` (exe_{cloud,tornado,
explosion,bunny,stock} + ir_*; cloud/tornado/explosion 64/96·112/384·32/160, bunny 80/528). `build-ferf/`
= fast-erf. build/ = stock, constants clean. Thesis 59 pp, builds clean. GPU at 350 W (idle).
Checklist: `results/campaign/CHECKLIST.md`. Plan: `docs/superpowers/plans/2026-06-12-section6-window-campaign.md`.

**DONE + documented:** G3 (3-env RIS), RR re-anchor, fast-erf, G8 (+re-anchor), G4 (cloud+bunny ncu),
G7 (JIT), G10 (tornado/explosion parity PASS via native-PLY+`opacities_0→sigma_t_0` rename), G5 (GAS).
Thesis-wired: sec:ris, fig:rr-depth, tab:wins (RR+fast-erf), Ch3 JIT/G7, sec:bottleneck+roofline,
deficit-framing removed.

**REMAINING — 350 W (timed):**
1. **G2 merge-ladder A/B** — shadow-transmittance (`merge-base main deprecated-shadow-transmittance-opt`↔`8c12af5`), skip-scan (`174777d~1`↔`174777d`), dedup (`f54deaa~1`↔`f54deaa`), fusion (`71ced87~1`↔`71ced87`). **8 worktree builds first (CPU/150 W)**, then interleaved A/B. Fills `tab:wins` ladder rows. Watch: incremental-active-prims ~16% may shrink at 64 active.
2. **G2 denoiser** effective-RMSE vs 2048-spp GT.
3. **(optional) comparison A** — after the pure-analog code fix above.

**REMAINING — 150 W / cap-immune:**
4. **G5b VRAM** (the memory gap): per-asset peak VRAM (`nvidia-smi --query-compute-apps=used_memory` poll), calibrated-vs-512 cap savings, ours-vs-Mitsuba.
5. **Render figures:** G1 bias triptych (ours-NEE|Mitsuba-NEE|GT — data ready, `scripts/plots/asset_triptych.py` is a template), showcase beauty montage, RIS equal-time-noise (banked `ris_seeds_meadow/`), icosphere N=3 sliver crop, denoiser triptych. (G10 triptych + g1_cloud_noise.png done.)
6. **G6** wavefront/adaptive confirms (need builds; huge-margin negatives, 150 W fine).
7. **Builds (CPU):** ladder worktrees + wavefront/adaptive — prep so a short 350 W window does only timing.

**THESIS WIRING TODO:** G1 (B + 59×) → Ch7 headline; `tab:wins` ladder rows (after G2); render figures → Ch5/showcase; G5b → Ch4/results.

**PENDING DECISIONS (Kacper):** G1 framing (lean: B + B-justified-59×; A optional via fix) — he was deciding; pure-analog code fix yes/no; merge `main` already done; deprecated-*/worktree cleanup is his.

---
## EXAMINER RE-VERIFICATION (2026-06-15, independent recompute from banked EXRs)
Recomputed from g1_seeds/ (16 seeds, 64 spp, RGB-avg convention) with system python3+OpenEXR:
- means: ours-MIS 0.3214, Mitsuba-analog 0.3201, Mitsuba-NEE 0.8199 — EXACT match.
- bias vs analog GT: Mitsuba-NEE +156.1%, ours-MIS +0.4% — EXACT match.
- k_raw: ours 1.985, mits 3898.6 -> raw ratio 1964x (~"2000x").
- k_clip999 (DOCUMENTED method = clip raw radiance at 99.9th pct THEN variance, run_g1_flat.sh:49):
  ours 1.887, mits 110.59 -> CLIP RATIO 58.6x = the 59x headline. REPRODUCES.
  NOTE: a different clip (clipping the per-pixel k array, run_g1_analog.sh:41) gives ~1520x, NOT 59x;
  the headline depends on the raw-radiance clip convention, which the flat/locked scripts use.
- flat-env: ours-analog 2.98x faster/sample, 4.99x higher variance, net 0.60x — EXACT match to g1_flat.md.
Verdict: headline + bias SUPPORTED, framing honest (env-IS scoped, NEE-bias disclosed, no hobble spin).
