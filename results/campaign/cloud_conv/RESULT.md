# Phase-1 cloud-meadow convergence (2026-06-25)

Showcase config: meadow env, sigma 7.5, albedo 0.9, HG 0.85, max_depth 128, BOX filter, cam 0.
spp ladder 64/256/1024, 8 seeds/cell, 3 arms. Data: cloud_conv/*.exr + times.csv.

## Exp2 — disagreement test (means vs spp)
| arm | 64 | 256 | 1024 |
|---|---|---|---|
| ours-MIS    | 0.3213 | 0.3215 | 0.3215 |
| Mitsuba-analog | 0.3260 | 0.3255 | 0.3227 |  (converging DOWN toward ours ~0.322)
| Mitsuba-NEE | 0.8200 | 0.8199 | 0.8199 |  (pinned at 0.82)
NEE and analog do NOT converge to a common mean (gap +154% at 1024 spp, constant). Two unbiased
estimators must agree in the limit; they don't => one is biased, and the furnace (Phase 0, analytic GT)
says it is NEE. ours-MIS sits ON analog (both ~0.32), confirming ours is the unbiased one.

## Exp3 — variance & equal-variance speedup (ours-MIS vs Mitsuba-analog, the unbiased pair)
raw per-pixel variance (~1/spp both): ours 3.09e-2/7.78e-3/1.94e-3 ; analog 2.03e2/5.00e1/1.24e1.
median render time (s): ours 7.6/31.8/124.6 ; analog 11.4/45.9/183.2 ; NEE 33.8/138.5/544.0.
- raw variance ratio (analog/ours) ~ 6400x ; ours also faster per sample (124.6s vs 183.2s at 1024).
- Equal-variance speedup (time to reach a common variance):
    RAW:     ~9369x  (firefly-dominated -- analog's bright-sun escapes)
    CLIPPED: ~82x    (99.9pct radiance clip, this ladder/box filter)
  Thesis headline 59x = clipped equal-quality at 64spp/g1 (gaussian filter). So the conservative,
  firefly-robust equal-quality speedup is ~59-82x; the raw is ~6400-9369x.

NOTE (NEE cost): NEE is ~3x SLOWER than analog (544s vs 183s at 1024spp) -- per-vertex shadow rays.
So even ignoring bias, ours-MIS dominates NEE on time AND (clipped) variance.

Plots: thesis/latex/figures/cloud_mean_convergence.pdf, cloud_equalvar.pdf
