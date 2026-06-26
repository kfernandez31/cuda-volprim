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

## D — equal-variance with DIFFERENT spp per side (committee methodology directive #6)
Equal-variance comparison does not require equal spp; each renderer runs to whatever spp reaches the
TARGET variance, and we compare wall-time. Variance ~ 1/spp on both arms (log-log slope -1.0 confirmed:
ours 64->1024 spp drops var 0.031->0.0019 = 16.3x for 16x spp; analog 202->12.4 = 16.3x). So:
- To reach ours-MIS's 1024-spp noise (raw var 1.94e-3, 125 s), Mitsuba-analog would need ~6.5M spp
  (~13 days at 183 s/1024spp) -> ~9000x equal-variance speedup (RAW).
- This raw figure is dominated by analog's rare bright-sun fireflies. The firefly-robust (99.9-pct-clip)
  figure is the conservative ~59x headline (single-spp variance ratio; clipped variance does not follow
  1/spp because fireflies dilute with spp, so it is reported as a same-spp ratio, not a curve
  extrapolation).
Honest range: ours-MIS is ~59x (firefly-robust) to ~9000x (raw) faster than Mitsuba-analog at equal
variance on the meadow. ALL experiment reports/thesis must state explicitly that equal-variance permits
different spp per side (this is the honest setup; equal-spp is only used where noted).

## E — plots (committee asks #10/#11/#12) DONE
- cloud_mean_convergence.pdf : mean vs spp (disagreement; NEE 0.82 vs analog/ours 0.32, never converge).
- cloud_equalvar.pdf         : raw variance vs samples (left) and vs wall-time (right); ours ~4 decades
  below analog, both ~1/spp; equal-variance = horizontal gap on the time panel.
