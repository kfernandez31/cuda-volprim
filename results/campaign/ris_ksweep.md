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

## Flat + studio — PENDING

Flat (`white_constant`) and studio (`ferndale_studio_01`, needs the SG_ENV wiring in
`test/scenes/cloud_validation.cpp`) complete the 3-point peakiness ladder and the `fig:ris-ksweep`
figure (its committed CSV `ris_ksweep.csv` stays header-only until all three columns exist — the
figure builder plots all three envs). Expected: flat *loses* (~2.5× worse, dev) — the honest
scene-dependence anchor; studio intermediate.
