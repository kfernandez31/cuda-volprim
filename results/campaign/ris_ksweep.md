# G3 — Product-RIS K-sweep (equal-quality vs MIS), per environment

Protocol per env: cloud scattering, `SG_CAM=0`, **64 spp pinned**, seeds 1–16, **7 arms interleaved
within each seed block** (MIS baseline + `--ris --ris-candidates K`, K ∈ {1,2,4,6,8,12}).
k = per-pixel inter-seed variance across 16 seeds (mean over pixels+channels) × spp; eff = k·t_median;
**speedup = eff_MIS / eff_K** (same env both sides → no energy matching). Seed-bootstrap 95 % CIs
(50 resamples). Per-seed EXRs kept (gitignored) so any re-analysis reuses images.

## Meadow (showcase, peak ≈ 2×10⁵) — run 2026-06-12, 350 W + clock lock

**Operating conditions were clean:** block means 7.8–8.1 s across all 16 seeds (no contention drift),
so unlike the RR sweep both ratios AND absolute times are usable as-is.

| arm | t_med | k | eff = k·t | speedup vs MIS | 95 % CI |
|---|---|---|---|---|---|
| MIS  | 9.784 s | 1.9842 | 19.412 | 1.000 | — |
| K=1  | 7.015 s | 2.3327 | 16.364 | 1.186 | [1.171, 1.194] |
| K=2  | 7.162 s | 1.9560 | 14.009 | 1.386 | [1.373, 1.394] |
| K=4  | 7.430 s | 1.7637 | 13.105 | **1.481** | [1.467, 1.490] |
| K=6  | 7.751 s | 1.6975 | 13.158 | **1.475** | [1.467, 1.483] |
| K=8  | 7.999 s | 1.6710 | 13.367 | 1.452 | [1.441, 1.459] |
| K=12 | 8.488 s | 1.6376 | 13.899 | 1.397 | [1.384, 1.406] |

**Findings (meadow).**
- **RIS wins ~1.48× equal-quality on the showcase**, sharpening the dev-era "~1.4×" to a CI'd number.
  The win decomposes into both halves: time (1 shadow ray vs 2: t 9.78→7.75 s at K=6, −21 %) *and*
  variance (k 1.98→1.70, −14 %).
- **The K-curve is a plateau over K=4–6**: 1.481 vs 1.475 with overlapping CIs — statistically tied.
  The thesis's "peaking near K=6" survives (spec G3 said it survives if the peak lands at 4 or 6);
  K=6 stays a defensible default, K=4 equally good. Costs rise monotonically with K (candidate
  generation), k falls monotonically (better proposals) — the product turns over after 6.
- **K=1 (plain env-IS NEE) already beats MIS 1.19×** on this env — pure single-shadow-ray time win
  despite *higher* variance (k 2.33 vs 1.98); the resampling (K≥2) is what buys the variance half.
- **Replication check:** the MIS arm's k = 1.9842 reproduces the RR sweep's depth-12 k = 1.98417
  (same config, independent run) to 4 decimals — the k pipeline is self-consistent.

## Flat (`white_constant`, peak 1×) — run 2026-06-13, 350 W + clock lock

| arm | t_med | k | eff = k·t | speedup vs MIS | 95 % CI |
|---|---|---|---|---|---|
| MIS  | 7.784 s | 0.0966 | 0.1226 | 1.000 | — |
| K=1  | 5.657 s | 1.5734 | 1.4522 | **0.084** | [0.084, 0.085] |
| K=2  | 5.737 s | 0.8380 | 0.7850 | 0.156 | [0.155, 0.157] |
| K=4  | 5.830 s | 0.4691 | 0.4436 | 0.275 | [0.273, 0.278] |
| K=6  | 5.872 s | 0.3467 | 0.3319 | 0.369 | [0.367, 0.371] |
| K=8  | 5.945 s | 0.2855 | 0.2771 | 0.443 | [0.438, 0.445] |
| K=12 | 6.084 s | 0.2237 | 0.2222 | **0.552** | [0.549, 0.556] |

**RIS loses on flat at every K** — best case (K=12) is still **1.8× worse** than MIS (0.552×); K=1 is
~12× worse. On a constant env MIS's variance is already tiny (k=0.097); RIS resampling only adds
variance (k 0.22–1.57 ≫ 0.097) with no peak to find. The honest scene-dependence anchor: **use MIS on
flat envs.** (Dev estimate "~2.5× worse"; measured 1.8× at the best K, worse at low K.)

## Studio (`ferndale_studio_01`, peak ≈ 538×, ~40 % top-0.1 %) — run 2026-06-13, 350 W + clock lock

| arm | t_med | k | eff = k·t | speedup vs MIS | 95 % CI |
|---|---|---|---|---|---|
| MIS  | 7.909 s | 1.0193 | 1.2329 | 1.000 | — |
| K=1  | 5.705 s | 1.4020 | 1.2199 | 1.008 | [1.003, 1.015] |
| K=2  | 5.864 s | 1.0889 | 0.9757 | 1.263 | [1.258, 1.268] |
| K=4  | 6.148 s | 0.9279 | 0.8708 | **1.413** | [1.407, 1.421] |
| K=6  | 6.383 s | 0.8739 | 0.8529 | **1.445** | [1.435, 1.454] |
| K=8  | 6.643 s | 0.8474 | 0.8593 | 1.432 | [1.424, 1.440] |
| K=12 | 7.117 s | 0.8212 | 0.8936 | 1.379 | [1.370, 1.388] |

**RIS wins ~1.45× on studio**, peaking at **K=6** (K=4/K=8 within CI). K=1 (plain env-IS NEE) is
statistically tied with MIS here (1.008×) — the resampling (K≥2) buys the win. Lands cleanly *between*
flat (loses) and meadow (1.49×).

## Meadow re-anchor at calibrated caps (64/96) — run 2026-06-13, 350 W + clock lock

Banked meadow **k stays** (rule R8); only timings re-measured (5 interleaved rounds) on the calibrated
cloud pair under the clean window. Absolutes dropped vs the 2026-06-12 sweep (MIS 9.78→7.81 s — faster
build + clean clocks); **speedups confirmed**:

| arm | t_med (new) | k (banked) | eff | speedup vs MIS |
|---|---|---|---|---|
| MIS  | 7.813 s | 1.9842 | 15.503 | 1.000 |
| K=1  | 5.563 s | 2.3327 | 12.977 | 1.195 |
| K=2  | 5.647 s | 1.9560 | 11.046 | 1.404 |
| K=4  | 5.904 s | 1.7637 | 10.413 | **1.489** |
| K=6  | 6.122 s | 1.6975 | 10.392 | **1.492** |
| K=8  | 6.381 s | 1.6710 | 10.663 | 1.454 |
| K=12 | 6.786 s | 1.6376 | 11.113 | 1.395 |

Matches the banked 1.481/1.475 to within ~0.01 — re-anchor confirms the dev number at clean clocks.
Clock sentinel over the whole window: min 930 / **p50 1635** / max 1800 MHz (one desktop-burst dip;
medians used). **0 cap overflows across all 289 renders.**

## fig:ris-ksweep — DONE (2026-06-13)

`ris_ksweep.csv` filled (6 K-points × 3 envs); `fig:ris-ksweep` regenerated. The figure tells the
**peakiness-monotonic** story: RIS's equal-quality speedup rises with env peakiness — **flat loses
(≤0.55×) → studio 1.45× → meadow 1.49×**, peaking at K=6 and **saturating** between studio (538×) and
meadow (1.5e5×) despite ~300× more peak. The thesis "peaking near K=6" claim holds on both winning
envs.
