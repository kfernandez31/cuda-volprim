# Section 6 experiment lineup — full-blast runbook

**Date:** 2026-06-10
**Status:** design approved; ready to turn into an implementation plan (runner scripts)
**Purpose:** the complete set of measurements to run in one reserved full-blast window on Piotr
Rybicki's RTX 3090, producing every quantitative number and figure for Chapter 6 (Performance
Engineering) and the headline results carried into Chapter 7.

This is the conceptual lineup. The runner scripts and exact CLI invocations are the subject of the
follow-up implementation plan.

---

## 1. Goal

Establish, at the single full-blast operating point, the performance story of Chapter 6:

1. The **headline**: the renderer closed an initial ~5× equal-quality deficit against Mitsuba and
   overtook it on the showcase, firefly-free (G1).
2. The **per-optimisation evidence** justifying each kept win (G2) and the one studied algorithmic win
   (RIS, G3).
3. The **boundedness diagnosis** that explains why the megakernel is the right shape (G4).
4. The **memory** results (G5), the **negative-result** numbers (G6), and the **Mitsuba overhead**
   comparison (G7).

Every reported number comes from this run; dev-time 150 W numbers in FINDINGS remain as record only.

## 2. Operating point & hardware

- **GPU:** NVIDIA RTX 3090 (Ampere, CC 8.6, 24 GB). Clocks **locked** at the card's sustained full
  clock via `nvidia-smi` (`-lgc`/`-lmc`) so timings are comparable across the session. Record the
  locked clock and driver/CUDA/OptiX versions in the run log.
- **Renderer:** release build (`-O3`, `--use_fast_math`), OptiX-IR compiled once.
- **Reference:** Mitsuba 3 (CUDA backend) on the same GPU. Two configs: **analog** (NEE disabled →
  unbiased ground-truth behaviour) and **MIS** (its default; biased +6.5 % on the furnace, but the
  realistic perf/firefly competitor).
- **Clock-stability check** (first thing): a short repeated render to confirm the locked clock holds
  and frame-time variance is < a few %.

## 3. Assets & scenes

**Assets (two, distinct roles):**

| Asset | Prims | Role | Caps |
|---|---|---|---|
| Disney cloud | 652 | primary showcase; carries the scene-dependent experiments | 128/128 (fits) |
| Stanford bunny | 25 600 | scaling/stress; tests that the wins generalise | **recompile to ≥320/≥560** (estimator-sized) first |

WDAS variants are **out of scope** for reported numbers; the cap-estimator table (`tab:overlap`,
already produced) covers the broader density spread.

**Scenes / lighting:**
- **Meadow HDR environment** — the showcase lighting; carries G1, the env-map side of G3, fireflies.
- **Constant / flat environment** — carries the furnace (unbiasedness) and the flat side of the RIS
  sweep (where RIS loses).
- Cloud is rendered under both; bunny under meadow (scaling) primarily.

## 4. Methodology (measurement protocol)

- **Equal-quality is reference-free.** Both this renderer and Mitsuba-analog are unbiased, so a finite-
  spp render's error *is* its noise. Measure noise as the **per-pixel inter-seed variance across 16
  seeds**; the efficiency metric is `k = (inter-seed RMS noise)² · N` (spp), and renderer X beats Y iff
  it reaches the same `k` in less time. No expensive converged reference is needed.
- **Bias is checked separately**, against **analytic** ground truth where it exists (absorption,
  furnace); scattering-mean agreement with Mitsuba-analog is already established in Chapter 5.
- **16 seeds** per configuration for noise/firefly statistics.
- **Fireflies:** report both clipped and unclipped noise plus the max-pixel/high-percentile, so the
  firefly-free advantage over Mitsuba-MIS is quantified rather than asserted.
- **Ablation protocol:** (a) **bare baseline → final** end-to-end (the gap-closing story); (b)
  **leave-one-out** from the all-on configuration (each kept M-mode win's marginal contribution). Only
  M-mode optimisations are toggled; C/S/I-mode items are argued, not ablated (see `tab:four-modes`).
- **Resolution:** the Chapter 5 validation resolution for renders; **256²** specifically for the `ncu`
  profile (proper grid fill — see FINDINGS §8.28).
- **Camera views:** 1–3 representative views for perf/ablations (view-independence is already
  established in Chapter 5).

## 5. Experiment groups

Each group lists its purpose, the configurations to run, and the figure/table it feeds.

### G1 — Headline cross-renderer  → `fig:showcase` (Ch 5 money-shot), Ch 6 intro + Ch 7
Configs (cloud-meadow, bunny-meadow): this renderer (final), Mitsuba-analog, Mitsuba-MIS.
Measure per config: raw frame-time at fixed spp; inter-seed noise (16 seeds) → `k`; clipped/unclipped
noise + max-pixel (fireflies). Derive the equal-quality speedup ratios.
Render the money-shot images: ours vs Mitsuba-analog at equal quality + a firefly crop vs Mitsuba-MIS.

### G2 — Optimisation ablations  → `tab:wins`, `fig:rr-depth`
Bare baseline → final end-to-end (report the cumulative gap close), plus leave-one-out for each kept
M-mode win: shadow-ray transmittance, skip per-bounce containment scan, dedup bounce-0 scan, any-hit
transmittance fusion, fast `erf`, denoiser. Each on cloud-meadow (key ones also on bunny for
generalisation); report frame-time and `k`.
- **fast `erf`** additionally needs a **bias** measurement: converged mean with fast vs accurate `erf`
  (the speed/accuracy trade).
- **RR-depth sweep** {4, 6, 8, 10, 12, 16}: frame-time and `k` per depth → `fig:rr-depth` (efficiency
  knee, justifying 12).

### G3 — Volumetric product-RIS  → `fig:ris-ksweep`, `sec:ris`
K-sweep {1, 2, 4, 6, 8, 12} on **cloud-meadow** (RIS wins) and **cloud-flat** (RIS loses): equal-
quality speedup vs plain MIS at each K. Plus a **furnace** unbiasedness re-confirm (flat env, albedo 1).

### G4 — Profiling & boundedness  → `sec:bottleneck`, `fig:roofline`
`ncu` on the render kernel (cloud + bunny @256²): achieved occupancy, eligible-warps/scheduler stats,
stall breakdown (long-scoreboard etc.), Speed-of-Light SM% vs DRAM%, register count. `nsys` timeline:
wall-clock split across trace / scatter / escape / shading. **Roofline:** arithmetic intensity vs
achieved GFLOP/s, plotted against the 3090's compute and memory roofs → `fig:roofline`.

### G5 — Memory  → `fig:gas-memory`, `sec:opt-memory`
GAS size before/after compaction per asset → `fig:gas-memory`. Peak device memory per asset. Per-ray
state footprint (megakernel register-resident vs the wavefront autopsy's ~352 B/ray). Optional:
primitive-count memory vs an equivalent NanoVDB voxel grid (cite if no grid available).

### G6 — Negative-result numbers  → `sec:autopsies`
Re-measure at full-blast the two that carry quantitative weight: **wavefront** slowdown (≥1 clean
config on the cloud — confirm the order-of-magnitude regression) and **adaptive sampling** net-loss
(equal-quality with vs without, cloud). The remaining autopsies (footprint-reduction null, exit-cache,
env-IS alias < 1 %, Owen–Sobol null) are cited from dev-time, not re-run.

### G7 — Mitsuba JIT / startup overhead  → `sec:bottleneck` / Ch 3 limitations (#96)
Time Mitsuba's kernel compilation + first launch vs steady-state per-frame, against our compile-once
OptiX-IR + launch. One-shot comparison; report the fixed startup cost Mitsuba pays.

## 6. Output artifacts (→ figure pipeline)

Runners write CSVs into `results/campaign/`; `scripts/plots/build_figures.sh` turns them into the
already-wired figures. Schemas (header-only today):

| Artifact | Columns | Feeds |
|---|---|---|
| `rr_depth.csv` | `rr_depth, frame_ms, noise, k` | `fig:rr-depth` |
| `ris_ksweep.csv` | `K, speedup_envmap, speedup_flat` | `fig:ris-ksweep` |
| `gas_memory.csv` | `asset, gas_mb_uncompacted, gas_mb_compacted` | `fig:gas-memory` |
| `roofline.csv` | `kernel, arith_intensity, achieved_gflops` (+ peak roofs as plotter constants) | `fig:roofline` (needs a dedicated log-log plotter) |
| `wins.csv` (new) | `optimization, mode, frame_ms, k, speedup` | `tab:wins` |
| `headline.csv` (new) | `renderer, asset, scene, frame_ms, noise, k_unclipped, k_clipped, maxpix` | G1 numbers / Ch 7 |

The three validation montages (`fig:absorption-ladder`, `fig:scattering-ladder`, `fig:showcase`) are
assembled from renders (G1 + the Ch 5 ladders), overwriting their placeholders.

## 7. Sequencing & rough GPU-time

Order to de-risk: (0) clock-stability + bunny cap recompile; (1) G4 profiling and G7 (quick, no seeds);
(2) G5 memory (quick); (3) G1 + G2 + G3 (the 16-seed bulk — the long pole); (4) G6.

Rough budget (order-of-magnitude, refine in the plan): the 16-seed ablation/headline/RIS bulk dominates
(~tens of short renders × 16 seeds × 2 assets for the key ones); profiling adds 1–2 h (`ncu` is slow);
memory/JIT are minutes. **Plan for an overnight (~8–12 h) window** with margin; it compresses by
dropping bunny from the per-optimisation ablations (keep bunny for headline/memory/profiling only).

## 8. Out of scope

WDAS assets for reported numbers; emissive assets (no emission support); the Gabor-noise extension;
re-running the cited C/S/I-mode optimisations; the Chapter 6 prose rework itself (tracked separately in
the handoff's "Ch 6 rework" queue).
