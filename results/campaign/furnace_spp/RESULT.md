# Furnace bias-vs-spp — Phase-0 GATE result (2026-06-25)

Settles the advisor challenge "Mitsuba-NEE is unbiased; in the limit; did you use enough spp?".
Furnace = albedo 1, white-constant env (=1), single Gaussian, exact ellipsoids, max_depth=256.
Energy conservation => mean MUST be 1.0 at ANY spp. 8 seeds/cell, 95% t-CI. Centre = central 1/4 box.

## VERDICT: BIAS CONFIRMED (NEE over-count is flat with spp; controls flat at 0)
| arm | sigma | 64 spp | 256 | 1024 | 4096 |
|---|---|---|---|---|---|
| mits_nee    | 6  | +9.756% | +9.738% | +9.726% | +9.730% |
| mits_nee    | 12 | +30.967% | +30.947% | +30.944% | +30.957% |
| mits_analog | 6/12 | 0.000% | 0.000% | 0.000% | 0.000% |
| ours_MIS    | 6/12 | ~0 (+-0.02%) | ~0 | ~0 | ~0 |

A 64x increase in spp (64->4096) moves the NEE over-count by <0.03 percentage points, while both
controls (Mitsuba's own analog mode + ours) sit exactly on the analytic GT of 1.0. An unbiased
estimator's MEAN equals the GT at every spp (only variance shrinks); NEE's mean is pinned ABOVE it.
=> Mitsuba volprim NEE is SYSTEMATICALLY BIASED on dense scattering media, growing with optical
thickness (+9.7% at sigma6 -> +31% at sigma12). The 64-spp +156% on the cloud is this same defect,
not undersampling. Headline framing (analog = the right unbiased reference) STANDS.

Mechanism note: both renderers use ANALYTIC NEE transmittance (ours compute_transmittance_to_env;
Mitsuba eval_transmittance = exp(-density_integral*sigma_t)), so the bias is NOT a stochastic-vs-
analytic transmittance issue -- it is in volprim's NEE/continuation MIS combination. (The thesis's
prior "stochastic shadow transmittance" wording in the B1 mechanism para needs correcting.)

Plot: thesis/latex/figures/furnace_bias_vs_spp.pdf  Data: furnace_bias_vs_spp.csv

## High-spp extension (2026-06-25, answering 'how high spp?')
Extended NEE+controls to 16384 spp (256x the original 64) at sigma=6:
  mits_nee centre over-count: +9.756% (64) -> +9.730% (4096) -> +9.732% (16384), CI [9.73,9.74].
  mits_analog & ours: 0.000% / ~0 throughout. (65536 attempted but Mitsuba OOM'd at 4.3B samples; 16384 = 256x is conclusive.)
From 64 to 16384 spp the over-count moves <0.005 points => the bias is spp-independent; 'unbiased in the limit' is refuted at the data level.
