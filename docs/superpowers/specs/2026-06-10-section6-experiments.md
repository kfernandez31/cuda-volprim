# Section 6 experiment lineup — full-blast runbook

**Date:** 2026-06-10 · **Revised:** 2026-06-10 after plan-review
(`thesis/reviews/2026-06-10-section6-plan-review.md`).
**Status:** design revised per review; ready to turn into an implementation plan (runner scripts).
**Purpose:** the complete set of measurements to run in one reserved full-blast window on Piotr
Rybicki's RTX 3090, producing every quantitative number and figure for Chapter 6 (Performance
Engineering) and the headline results carried into Chapter 7.

This is the conceptual lineup. Runner scripts and exact CLI invocations are the follow-up
implementation plan. **Read §0 first — the run is invalid without those preconditions.**

---

## 0. Preconditions (Step 0 — before any timed run)

These are renderer-code + setup changes, not runner-script work. The run cannot produce its claims
until they are done, in this order:

**STATUS (2026-06-11): §0.1, §0.2, §0.3 DONE** (clock-independent, runnable without the window).
Record: `results/campaign/caps_per_asset.md`. §0.4 (clock lock) is window-only; §0.5 (icosphere port)
is the `feature/icosphere-gas` branch.

1. ✅ **CLI status (corrected — review act-3 was checking the wrong binary).** The campaign runs through
   **`test_runner`, whose CLI already exposes `--ris` and `--ris-candidates K`** (verified at
   `test/test_runner.cpp:125-126`), so **G3 is runnable as-is**. The gap is only in the standalone app
   (`src/thesis/host/app/config.cpp` lacks `--ris`); fix that *only if* that binary is shipped/used.
   Adaptive sampling is a clean **compile-time rebuild-toggle** (`ENABLE_ADAPTIVE_SAMPLING`,
   `constants.cuh:187`) — exactly what the spec permits for the one-shot G6-adaptive autopsy; no runtime
   flag needed (a converged-reference RMSE measurement is a single rebuild, not an interactive sweep).
2. ✅ **Per-asset cap recompiles VERIFIED** (estimator-sized, `caps_table.csv`): bunny **320/496**,
   tornado **112/432**, explosion **32/176**. Cloud stays 128/128. Each predicted cap is now
   *necessary* (stock 128/128 overflows under the scattering stress) and *sufficient* (`Cap check: 0
   overflows` at the predicted caps — and 0 drops ⟺ image identical to unbounded caps). The new assets
   are converted from Jorge's native npy via `tools/refs/npy_asset_to_ply.py`. **Finding:** the
   estimator is a conservative *whole-bbox* bound; explosion is clean at stock under absorption from all
   views but shows its predicted *marginal* overflow (4 drops) under scattering — so **scattering is the
   binding stress**, and caps are sized to the estimator for camera/path-independent safety. Full record:
   `results/campaign/caps_per_asset.md`.
3. ✅ **Re-gated every per-asset binary**: furnace (single-Gaussian albedo-1, constant env) @ 1024 spp
   PASS (bias + structure) on each recompile, confirming the buffer-size edit didn't break correctness
   (cap-independent; complements the `caps_ab.md` numerical-equivalence result). The cloud-diff half of
   the original gate is moot for the new assets (each binary renders a *different* asset, and the
   explosion build's 32 active is below the cloud's 45) — the per-asset 0-overflow log + furnace is the
   correct gate.
4. **Clock lock + stability check.** `nvidia-smi -lgc/-lmc` to the sustained full clock; a short repeated
   render to confirm frame-time variance < a few %. (Note: `ncu` ignores this and locks to *base* clock
   via `--clock-control`; its metrics are ratios, so fine — but record it, don't claim "full-blast" for
   ncu rows.)
5. **(Optional, G8 only) Port the icosphere GAS (CODE).** Lift `Icosphere<N>`
   (`include/thesis/host/geometry/mesh.h`) + `TriangleGAS` (`gas.h`) from commit `eb5372f` into the
   current renderer behind a build switch + subdivision level `N`; entries then come from triangle hits,
   exits stay analytic. Real engineering, not a flag — greenlight before committing the window to it.

## 1. Goal

Establish, at the single full-blast operating point, the performance story of Chapter 6:

1. The **headline**: under **flat lighting** the renderer closed an initial ~5.5× equal-quality deficit
   against Mitsuba-analog (§8.5: 1.93× per-spp × 2.85× noise); on the **environment-lit showcase** it was
   already ahead pre-optimisation (§8.11/§8.15) and stays ahead, firefly-free. **Both halves must be
   measured** — the deficit-closure number is flat-env (G1 flat rung), not meadow.
2. **Per-optimisation evidence** for each kept win (G2) and the one studied algorithmic win (RIS, G3).
3. The **boundedness diagnosis** for why the megakernel is the right shape (G4).
4. **Memory** (G5), the **negative-result** numbers (G6), the **Mitsuba overhead** comparison (G7).

**"Every reported number from this run"** has two honest carve-outs: (a) cited dev-time *negatives*
(G6's non-re-run set); (b) Chapter 5's correctness *images and RMSE* — clock-independent artifacts, so
reusing dev renders is legitimate, but say so (and either schedule the brute-force analytic rungs that
`fig:absorption-ladder`'s caption promises, or fix the caption — full-review M3).

## 2. Operating point & hardware

- **GPU:** RTX 3090 (Ampere, CC 8.6, 24 GB). Clocks locked at the sustained full clock; record locked
  clock + driver/CUDA/OptiX versions.
- **Renderer:** release build (`-O3`, `--use_fast_math`), OptiX-IR compiled once.
- **Reference, two configs:**
  - **Mitsuba-analog** (NEE disabled → unbiased): the **only** valid equal-quality opponent.
  - **Mitsuba-MIS** (its default): converges to a biased image (+6.5 % furnace §8.1; +155 % on the
    showcase §8.11). **Never** compare equal-quality against it — matching its noise means "reaching the
    wrong answer faster." Report vs MIS only: **per-spp time, firefly statistics, and its bias.**

## 3. Assets & scenes

**Final lineup per Jorge (mail, 2026-06-11): "Disney Cloud, Bunny, Tornado, Explosion are a good mix
with different levels of difficulty."** Caps per asset from `estimate_caps.py` (margin 1.25, ×16):

| Asset | Prims | Role | Caps (active/hit) |
|---|---|---|---|
| Disney cloud | 652 | primary; scene-dependent experiments + flat-env headline | 128/128 (fits; active-set A/B 64↔128 measured timing-neutral, `caps_ab.md`) |
| Stanford bunny | 25 600 | scaling/stress; do the wins generalise | recompile **320/496** |
| Tornado | 768 | mid-difficulty; elongated funnel — worst ray-entry geometry (340 entries!) | recompile **112/432** |
| Explosion | 1024 | mid-difficulty | recompile **32/176** (marginal hit-buffer overflow at 128) |

**Both new assets overflow the stock 128 hit buffer** — per-asset recompiles are mandatory (§0.2), and
the lineup gives the cap-estimator workflow four real datapoints. **Caveat (confirm with Jorge):** the
explosion is a combustion asset; this renderer has no emission, so it renders as a scattering-only
plume under env lighting — presumably intended (he knows the scope), but confirm the expected look.
The Mitsuba-parity gate (below) applies to tornado/explosion as it does to the bunny.

**Bunny gate (act-7):** before any cross-renderer bunny number, establish converged-mean agreement
with Mitsuba (the §8.25 energy-ratio method, 0.9999 on wdas8, is the template). The asset-side Mitsuba
script supports constant-env only and the `asset_validation` camera was found vertically flipped vs
Mitsuba — resolve both, or scope bunny to **ours-internal scaling only** (no equal-quality-vs-Mitsuba
claim). WDAS variants out of scope for reported numbers (`tab:overlap` covers the density spread).

**Scenes — three-point peakiness ladder (real HDRIs; the peakiness numbers must come from a committed
script so the figure x-axis is reproducible):**
- **Flat / constant** (`white_constant`, peak 1×) — furnace + the RIS low end (loses) + **the flat-env
  headline rung**.
- **Studio** (`ferndale_studio_01`, peak ≈ 700×, ~47 % energy in top 0.1 %) — mid-peak; RIS middle
  point; a second showcase scene. CC0 Poly Haven, `scripts/tools/fetch_envmaps.sh`.
- **Meadow** (`meadow_2_4k`, peak ≈ 2×10⁵×, ~74 %) — hard sun; the showcase; RIS wins, fireflies appear.

## 4. Methodology

**The "final" config is frozen, two variants (act-6).** Every $k$-measured number uses
**final-validation**: exact `erf`, box pixel filter, no firefly clamp, no denoiser, MIS (the *unbiased*
config). **final-showcase** (denoiser, etc.) is for **images only**, never for a noise constant. State
which variant feeds each figure.

**Equal-quality is reference-free — but only for unbiased, fixed-N outputs.** For final-validation and
Mitsuba-analog, both unbiased, a finite-spp render's error *is* its noise; measure per-pixel inter-seed
variance across **16 seeds** and report $k = (\text{RMS noise})^2 \cdot N$ (thesis k-convention, =
FINDINGS' $k^2$ — note it when transcribing). X beats Y iff it reaches the same $k$ in less time.
Excluded from $k$, by construction:
- **Mitsuba-MIS** (biased — §2),
- **denoiser** and **adaptive sampling** (biased / variable-N → inter-seed variance is blind to them;
  $k$ ill-defined when N varies per pixel). For these use **converged-reference RMSE** (one ~2048-spp
  uniform GT per scene, the §8.22/§8.30 machinery), labelled **"effective"**.

**Tails & uncertainty.** Mitsuba-analog-meadow's $k$ is spp-dependent (fireflies converge slower than
1/√N): **pin and report the measurement spp**, lean on clipped-$k$ + percentiles for tail-heavy configs.
Report **seed-bootstrap CIs on every equal-quality ratio**; for ≤3 % effects (fusion, fast-erf) keep
**interleaved A/B** ordering even at locked clocks (locking removes thermal drift, not all jitter —
§8.35 saw ~9 % jitter swamp a small effect).

**Ablation protocol (act-2) — leave-one-out-from-all-on is *not* executable** (none of the six wins is a
runtime toggle; the shadow-transmittance "before" code is deleted and any-hit fusion is entangled with
it). Instead:
- **Merge-commit ladder (default).** For each kept win, check out its merge commit and parent (the
  `deprecated-*` branches restore these) and re-run the *same* A/B at locked clocks. Reproduces the
  **sequential-at-historical-point** numbers `tab:wins` already cites — zero new code.
- **Leave-one-out at final** only where a real toggle exists: denoiser (`--denoise`), fast-erf (CMake),
  RR (`--rr-depth`), RIS (`--ris`, post-§0).
- **Label the semantics in `tab:wins`** — sequential-historical ≠ marginal-at-final (the shadow rewrite
  was ~85 %-of-frame when measured; its marginal at final differs).
- **bare → final** endpoints: pin the bare SHA (pre-§8.16 validated state, RR at its historical default
  **5**); the 5→12 retune is part of the cumulative story.

**Bias gates:** furnace (reference-free, scatter-side), re-confirmed under RIS; absorption analytic
gates (dev artifacts); fast-erf via converged means; + the §0.3 re-gate after any binary change.

**Resolution (pin it — Ch 5 names none):** dev used **800² cloud / 512² assets**; adopt those and state
them. `ncu` at **256²** (grid-fill, §8.28). **Camera views:** 1–3 representative for perf (view-
independence already shown in Ch 5).

## 5. Experiment groups

### G1 — Headline cross-renderer → Ch 6 intro + Ch 7, `fig:showcase` (Ch 5)
- **Flat-env rung (the headline source, act-1):** constant-env cloud × {bare-baseline, final-validation,
  Mitsuba-analog}, 16 seeds + Mitsuba per-spp time → the ~5.5× deficit-closure number, and the in-run
  evidence for Ch 6's "the equal-quality comparison inverts" (`06:57-59`, currently a §8.16/§8.23
  *extrapolation*).
- **Showcase rung:** meadow cloud × {final-validation, Mitsuba-analog} for equal-quality + per-spp;
  Mitsuba-MIS for per-spp + fireflies + bias (not equal-quality). Pin which binary "final" is — RIS
  on/off changes it ~1.4×.
- **Bunny:** meadow × {final-validation, Mitsuba-analog} **only after the §3 gate**; else ours-internal.
- **Fireflies:** clipped + unclipped noise, **p99.9 / p99.99 + max** (max alone is seed-unstable).
- **Money-shot images** (final-showcase): ours vs Mitsuba-analog at equal quality + a firefly crop vs MIS.

### G2 — Optimisation ablations → `tab:wins`, `fig:rr-depth`
Merge-commit ladder for the six wins (shadow-transmittance, skip-scan, dedup-bounce-0, any-hit fusion,
fast-erf, denoiser) + bare→final endpoints; semantics labelled (§4). **RR-depth sweep {5, 6, 8, 10, 12,
16}** — include **5** (the anchor of the cited 5→12 claim, §8.33). fast-erf **bias** gate (converged
mean, fast vs exact) shares the converged-reference machinery. Report frame-time + $k$ (final-validation).

**RR-sweep status (2026-06-12): RUN ONCE, k FINAL, timings provisional — POSSIBLE TIMING-ONLY RERUN.**
The 6×16-seed sweep ran at 350 W/locked clocks but a desktop-session burst (Prybicki's browser)
contaminated blocks ~8–9 (block means 7.0→16.9 s, recovering by s10–12). Per-block-normalized relative
times are clean (CV 3–5 %/depth, t16/t5 = 1.30 ± 0.06 SEM, 3/60 monotonicity violations) so the
`fig:rr-depth` efficiency knee stands; **absolute** frame times are anchored to the cleanest
pre-contention blocks only. *If* publication-clean absolutes are wanted: timing-only rerun on a quiet
GPU, ~10 min, **reuses the banked per-seed EXRs for k** (k is image-derived, contention-immune — no
re-derivation). Images + times: `/tmp/rr/` → banked record `results/campaign/rr_depth.md`.

### G3 — Volumetric product-RIS → `fig:ris-ksweep`, `sec:ris`
K-sweep **{1, 2, 4, 6, 8, 12}** on the cloud across flat → studio → meadow; equal-quality speedup vs MIS
**within each env** (same map both sides → no energy matching needed). K=1 = plain env-IS NEE
(consistency anchor, §8.37). The furnace re-confirm doubles as the **post-§0 RIS regression gate**. Let
the data set the default K (dev measured {2,4,8,16}, K=6 interpolated); `06:111` "peaking near K=6"
survives if the peak lands at 4 or 6.
**Unbiasedness on the showcase env: CONFIRMED (2026-06-10, 150 W, clock-independent → final).** Cloud
scattering on the meadow, MIS vs RIS-K6 at 1024 spp: converged means agree to within the MC-noise floor
(signed-mean Δ 1.9e-4, vs a same-estimator noise floor of 1.1e-4), and RIS's per-pixel `|Δ|` is
*lower* (2.98e-2 vs 3.21e-2) — consistent with its variance-reduction win, not a bias. Extends §8.37
(constant-env) to the showcase. **So the window needs only the RIS perf K-sweep, not a correctness
gate.** Ch 6 `sec:ris` can strengthen "validated unbiased … on a constant environment" → "…and on the
environment-lit showcase" (Ch 6 pass).

### G4 — Profiling & boundedness → `sec:bottleneck`, `fig:roofline`
**`ncu` is the load-bearing source** (cloud + bunny @256²): occupancy ~22 %, eligible-warps,
long_scoreboard-dominant stalls, SOL SM%/DRAM%, 114 regs, 82–98 % cache residency (§8.28). **Drop the
nsys trace/scatter/escape/shade split (act-8)** — the bounce loop is one megakernel; nsys resolves only
kernel-level (render vs denoise vs I/O). If a stage split is wanted, plan a `clock64()`-instrumented
build and present it as such. **Roofline = non-saturation visual only** (SM 34.5 % / DRAM 1.1 % → point
sits far under both roofs → latency-bound); caption it that way (else readers misread "under the memory
roof" as bandwidth-bound — the exact error `sec:bottleneck` refutes), and note GFLOP/s undercounts the
SFU/erf-heavy mix. **New work:** `fig:roofline` float (no chapter has it yet) + a dedicated log-log
plotter (not "already wired").

### G5 — Memory → `sec:opt-memory` (no dedicated figure)
**DONE (cloud + bunny, clock-independent → final):** AS footprint cloud 0.198→0.103 MB, bunny
7.72→3.97 MB; bunny fired the overflow counter at 128/128 (873k drops), confirming detection + the
estimator's 320/496 prediction.
**Compaction is demoted** — Mitsuba compacts with the same flags, so it is *(S)* standard practice, not
a result; no dedicated figure (the old `fig:gas-memory` was removed from Ch 6, `sec:opt-memory` now
states the footprint in one clause). The real memory result is the **per-ray state** (megakernel-resident
vs the wavefront ~352 B/ray) plus **absolute compactness** (sub-MB–few-MB AS vs millions of voxels;
NanoVDB comparison, cite if unavailable). The `gas_memory.csv` data stays as the **analytic baseline for
G8**. Still TODO: **peak VRAM** per asset (`cudaMemGetInfo` snapshot).

### G6 — Negative-result numbers → `sec:autopsies`
- **Wavefront:** A/B **within** `feature/wavefront-phase1` (`THESIS_WAVEFRONT` ON vs OFF at the *same*
  commit) — not branch-vs-main (which absorbs post-fork changes). One full-blast confirmation point + the
  cited dev range (100–1400×).
- **Adaptive:** **converged-reference RMSE** (its dev verdict ~2× slower at equal quality came from
  RMSE-vs-GT; the noise-only $k$ would hide its bias and mis-rank it). Needs the restored flag or a
  rebuild-toggle (§0.1).
- Rest (footprint-null, exit-cache, env-IS alias <1 %, Owen–Sobol) cited from dev-time.

### G7 — Mitsuba JIT / startup overhead → `sec:bottleneck` / Ch 3 (#96)
One-shot: Mitsuba compile + first launch vs steady-state, vs our OptiX-IR load + launch. State
explicitly that steady-state per-frame numbers elsewhere **exclude startup on both sides** (ours: IR
load; Mitsuba: JIT), so it is not double-counted in any $k\cdot t$.

### G8 — Analytic vs tessellated (icosphere) spheres → `tab:wins` + new figure; **reclassifies the Ch 6 (I)→(M) item**
The reference (DSYG) intersects each primitive as a **tessellated icosphere**; this renderer uses
OptiX's **built-in analytic sphere**. Ch 6 currently files this under *(I) infeasible to ablate* (the old
tessellated path predated the current architecture, `tab:four-modes` + `sec:reasoned`). It is in fact
**feasible**: port *only* the historical icosphere **GAS** into the current renderer — the rest (IAS
instancing, any-hit entry collection, analytic exit, optical-depth integration) is unchanged — giving a
fair A/B where *only the per-primitive geometry differs*. Resurrect `Icosphere<N>`
(`include/thesis/host/geometry/mesh.h`) + `TriangleGAS` (`gas.h`), both at commit **`eb5372f`** (the
pre-anyhit checkpoint), behind a build switch + subdivision level `N`.
- **Sweep** `N ∈ {0,1,2,3}` (12 → 642 vertices) vs the analytic sphere, same cloud scene/seeds.
- **Three axes:** (a) **perf** — frame time + equal-quality `k` (built-in HW sphere intersector vs
  triangle-BVH traversal); **clock-dependent → in the window**. (b) **accuracy** — RMSE vs the analytic
  render, which is the *exact* ground truth here (the primitive truly is a sphere), so the discrepancy is
  pure faceting error at each `N`; **clock-independent → runnable now**. (c) **GAS size** per `N`;
  clock-independent.
- **Expected shape (hypothesis; let the data decide):** analytic dominates — exact *and* faster — but the
  measured magnitude is the result, and low-`N` icospheres may be faster-but-faceted (the accuracy×perf
  trade is the story). Directly benchmarks our choice against the reference's.
- **Memory caveat (what the standalone G5 result does *not* answer).** The G5 `gas_memory.csv`
  (cloud+bunny, analytic, compacted) is the *analytic-self* half (uncompacted→compacted), **not** an
  analytic-vs-tessellated result. **Compaction is a wash:** Mitsuba compacts with the *same* flags
  (`ALLOW_COMPACTION | PREFER_FAST_TRACE` + an `optixAccelCompact` pass — verified in
  `~/jorge/mitsuba3/include/mitsuba/render/optix/shapes.h:153,180-214` and
  `src/render/scene_optix.inl:166`), so it is standard practice, not a differentiator. The real
  memory comparison is the **geometry**: our tiny analytic builtin-sphere GAS (instanced) vs the
  tessellated icosphere. **TODO when G8 runs:** determine Mitsuba's icosphere **degree** and whether it
  **instances one icosphere or tessellates per-primitive** (the latter → ~N× geometry, a large analytic
  win) from the `ellipsoids` shape plugin in `~/jorge/mitsuba3` (source is local → determinable now,
  no guessing); the IAS numbers already measured carry over unchanged (instance records are
  geometry-independent).
- **This is a code task (§0.5), not a flag flip.** If the port proves costly, fall back to the existing
  (I)-mode argument and leave the item infeasible.

## 6. Output artifacts (→ figure pipeline)

| Artifact | Columns | Feeds | Note |
|---|---|---|---|
| `rr_depth.csv` | `rr_depth, frame_ms, k, eff` | `fig:rr-depth` | **plot efficiency $k\cdot t$, not frame_ms** (frame_ms is monotone → no knee, contradicts the caption). Committed CSV currently `…,rmse` — reconcile. |
| `ris_ksweep.csv` | `K, speedup_flat, speedup_studio, speedup_meadow` | `fig:ris-ksweep` | matches |
| `gas_memory.csv` | `asset, gas_mb_uncompacted, gas_mb_compacted` | (no figure — demoted) | DONE; analytic baseline for G8 |
| `roofline.csv` | `kernel, arith_intensity, achieved_gflops` (+ roofs) | `fig:roofline` | **new plotter + new float** |
| `wins.csv` (new) | `optimization, semantics, frame_ms, k, speedup` | `tab:wins` | `semantics` ∈ {sequential, marginal} |
| `headline.csv` (new) | `renderer, asset, env, config, frame_ms, k, k_clipped, p99_9, maxpix` | G1 / Ch 7 | env includes `flat` |
| `effective.csv` (new) | `feature, scene, rmse_vs_gt, time` | denoiser/adaptive | "effective", not $k$ |
| `icosphere.csv` (new) | `subdiv_N, n_verts, frame_ms, k, rmse_vs_analytic, gas_kb` | G8 fig + `tab:wins` | analytic = exact GT |

Validation montages (`fig:absorption-ladder`, `fig:scattering-ladder`, `fig:showcase`) are assembled
from renders (G1 + Ch 5 ladders, the latter clock-independent — §1 carve-out).

## 7. Sequencing & rough GPU-time

(0) §0 preconditions (CLI restore, caps recompile to 320/496, re-gate, clock lock). (1) G4 ncu + G7
(quick). (2) G5 memory (quick). (3) the 16-seed bulk — G1 (incl. flat rung) + G2 merge-ladder + G3.
(4) G6.

Budget: plausible but tight. Long poles, in order: the 16-seed bulk, then **Mitsuba-analog on the
meadow** (high spp × 16 seeds for tail-stable clipped-$k$; bunny-analog adds more). Flat-env cells are
cheap. Compression valve: drop bunny from the per-win ablations (keep it for headline/memory/profiling).
Plan an overnight (~8–12 h) window.

## 8. Open decision & out of scope

- **Ch 7 R-set reconciliation (act-9) — PARTIALLY RESOLVED by Jorge's lineup (2026-06-11).** The asset
  set is now fixed: **cloud, bunny, tornado, explosion** (replaces R2's smoke/embergen idea). G1/G5
  extend to the two new assets after their cap recompiles + Mitsuba-parity gates. Still open: R3
  (time/memory vs N `stress_N` sweep) and R7's Mitsuba-side peak VRAM — add or rescope before the
  window.
- **[DECIDE] G8 icosphere A/B (effort-gated).** High value — it turns the analytic-vs-tessellated choice
  from an *argued* (I)-mode item into a *measured* (M)-mode result that directly benchmarks the
  reference's tessellated approach. But it needs the §0.5 code port. Greenlight the port, or keep the
  (I)-mode argument. **If it lands, Ch 6 changes:** move the row out of `tab:four-modes`'s (I) column,
  rewrite the analytic-vs-tessellated paragraph in `sec:reasoned`, and add the result — queued for the
  Ch 6 rework.
- **Out of scope:** WDAS reported numbers, emissive assets, the Gabor extension, re-running cited
  C/S/I-mode optimisations, and the Ch 6 prose rework (handoff "Ch 6 rework" queue).

---

## Appendix: run log — exact commands, cap-sensitivity A/B (2026-06-11)

Reproducible recipe for the experiment recorded in `results/campaign/caps_ab.md` (and the pattern for
any multi-build A/B — `OPTIXIR_PATH` is baked absolute at compile time, so builds are swapped by
copying the exe + `device_program.optixir` pair in place):

```bash
# 1. Build the 64-cap variant and stash the pair
sed -i 's/constexpr size_t MAX_ACTIVE_PRIMS = 128;/constexpr size_t MAX_ACTIVE_PRIMS = 64;/' \
    device/core/constants.cuh
cmake --build build --target test_runner -j
mkdir -p /tmp/ab
cp build/bin/Release/test_runner /tmp/ab/exe_64; cp build/device_program.optixir /tmp/ab/ir_64

# 2. Overflow probe at 64 (expect no warning; post-480f812 expect "Cap check: 0 overflows")
build/bin/Release/test_runner --scene cloud_asset_validation --spp 1 --width 32 --height 32 \
    --output /tmp/cap64_val.exr

# 3. Correctness gate: 1024-spp seed-42 scattering @64 vs the saved @128 render (same seed/spp)
SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering \
    --spp 1024 --width 256 --height 256          # NB: asset scenes IGNORE --width/--height (900x600)
tools/refs/.venv/bin/python tools/refs/exr_diff.py /tmp/mis.exr \
    test_results/cloud_asset_scattering/0000.exr  # gate: signed-mean ~1e-10, max|d| ~7e-5 (FMA reorder)

# 4. Build the 128 variant, stash the pair (revert the sed, rebuild, cp exe_128/ir_128)

# 5. Interleaved timing A/B (3 rounds, alternating builds under the SAME GPU state)
for i in 1 2 3; do for v in 128 64; do
  cp /tmp/ab/exe_$v build/bin/Release/test_runner; cp /tmp/ab/ir_$v build/device_program.optixir
  SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering \
      --spp 64 --width 256 --height 256 --output /tmp/ab_r.exr 2>&1 | grep "Total time"
done; done

# 6. Restore the canonical 128 build
cp /tmp/ab/exe_128 build/bin/Release/test_runner; cp /tmp/ab/ir_128 build/device_program.optixir
```

Lesson encoded in the protocol: a naive before/after across sessions showed a phantom 2× regression —
ambient GPU state (a desktop session on the card) swings per-spp time ~3× at the un-pinned 150 W
point. Interleave or lock clocks; never compare timings across sessions.
