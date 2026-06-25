# Furnace test of volprim_prb NEE — exact protocol & result (to relay to Jorge)

**Question.** Is `volprim_prb`'s next-event estimator (`use_nee=True`) energy-biased on a scattering
Gaussian, or does its apparent over-count vanish as spp → ∞ (i.e. is it just undersampling)?

## The test: a furnace (radiative equilibrium)
A single Gaussian with **single-scattering albedo = 1** (no absorption) embedded in a **uniform
environment of radiance 1** (constant emitter, value 1 from all directions). By energy conservation the
medium is in radiative equilibrium, so the in-scattered radiance equals the background and **every pixel
must equal exactly 1.0** — analytically, with **no** free parameters (no reference render, no density
calibration, no asset). Crucially this holds at **any** sample count: an unbiased estimator's *mean* is
1.0 at every spp; only its *variance* shrinks. So a converged mean ≠ 1.0 is **bias**, and a mean that
stays ≠ 1.0 as spp grows cannot be undersampling.

## Exact scene (Mitsuba 3, your `volprim` build)
- **Integrator:** `volprim_prb`, `max_depth = 256`, `kernel_type = "gaussian"`,
  `solver_type = "bisection"`, isotropic phase (no `phasefunction` override). Only `use_nee` is toggled.
- **Primitive:** `{"type": "ellipsoids"}` (exact analytic ray–ellipsoid, *not* the tessellated mesh),
  N=1, `center=(0,0,0)`, `scale=(1,1,1)`, identity quaternion, `extent=3.0` (3σ kernel support),
  `albedo` = `TensorXf([1,1,1])`, `sigma_t` = **6.0** and **12.0** (the per-primitive `sigma_t` attribute
  directly; with scale 1 this gives line-integral optical depth τ(d) = σ_t/(2π)·exp(−d²/2), i.e. peak
  τ(0)=σ_t/2π — the same convention our CUDA `--sigma-multiplier` uses).
- **Emitter:** `{"type":"constant","radiance":{"type":"uniform","value":1.0}}`.
- **Sensor:** `orthographic`, 256×256 `hdrfilm` (rgb, float32, gaussian rfilter), `independent` sampler,
  viewport spanning [−3,3]² (ortho height 6.0), camera at distance 5 along +Z looking at the origin.
- **Sampling:** spp ∈ {64, 256, 1024, 4096, 16384}; 8 independent seeds per cell (4 at 16384).

## Arms
1. **Mitsuba NEE** — `use_nee=True` (under test).
2. **Mitsuba analog** — `use_nee=False`, *same integrator/scene* (control; must return 1.0).
3. **Our CUDA renderer** (MIS) — independent control on the matched scene (must return 1.0).

## Metric
Per pixel = RGB-averaged radiance. **Centre over-count** = (mean over the central quarter box —
rows h/2±h/8, cols w/2±w/8 — minus 1) in %, where the in-scatter peaks. (Whole-image mean also tracked.)
Aggregated over seeds with a 95% t-interval.

## Result (centre over-count, %)
| spp | NEE σ=6 | NEE σ=12 | analog (σ=6/12) | ours (σ=6/12) |
|----:|--------:|---------:|----------------:|--------------:|
| 64    | +9.76 | +30.97 | 0.000 | ≈0 (±0.02) |
| 256   | +9.74 | +30.95 | 0.000 | ≈0 |
| 1024  | +9.73 | +30.94 | 0.000 | ≈0 |
| 4096  | +9.73 | +30.96 | 0.000 | ≈0 |
| 16384 | +9.73 | —      | 0.000 | ≈0 |

A **256× increase in spp (64→16384) moves the NEE over-count by <0.005 points.** Both controls —
including your **own analog mode**, same integrator and scene with only `use_nee` flipped — sit on
**0.000%** at every spp, so the furnace is valid (env=1 and albedo=1 confirmed) and the defect is
isolated to the next-event path. The over-count grows with optical thickness (≈+9.7% at σ=6 → +31% at
σ=12) and, on the dense cloud (albedo 0.9, overlap ~45), reaches the +156% we reported.

## What we have already ruled out
- **Undersampling / "in the limit":** flat to 16384 spp (above).
- **Path-depth truncation:** identical at `max_depth` 32 vs 256 (albedo=1 has no absorption termination,
  so we ran 256; analog at 256 = exactly 1.0, so depth is sufficient).
- **Stochastic transmittance:** NEE's shadow transmittance is **analytic** in volprim
  (`eval_transmittance`: `tr = exp(−density_integral·sigma_t)`, product over primitives) — as in ours —
  so this is not a transmittance-variance effect. The surplus therefore appears to enter in how the NEE
  emitter-sampling term and the emitter-interaction/continuation term are MIS-combined.

## Reproduce (from the thesis repo)
```bash
# Mitsuba NEE vs analog, full spp sweep (one JIT-amortised process per arm/sigma), appends a CSV:
for arm in nee analog; do nee=$([ $arm = nee ] && echo 1 || echo 0)
  SG_ARM=mits_$arm SG_NEE=$nee SG_ALBEDO=1.0 SG_ENV=white_constant SG_SIGMA=6 SG_MAX_DEPTH=256 \
    SG_SHAPE=ellipsoids SG_SPPS="64 256 1024 4096 16384" SG_SEEDS="0 1 2 3 4 5 6 7" \
    SG_CSV=/tmp/furnace.csv \
    tools/refs/with_jorge_mitsuba.sh tools/refs/.venv/bin/python tools/refs/render_single_gaussian_via_prb.py
done
```
Full driver: `scripts/campaign/run_furnace_spp_sweep.sh`; data `results/campaign/furnace_spp/`;
plot `thesis/latex/figures/furnace_bias_vs_spp.pdf`.

## The question for Jorge
Given that the *same* integrator with `use_nee=False` returns exactly 1.0 at every spp, but `use_nee=True`
plateaus at +9.7% (σ=6) / +31% (σ=12) and stays there to 16384 spp — **where does the NEE path inject the
surplus on a conservative (albedo=1) medium?** Is this a known edge case of the prb NEE/MIS combination
at high albedo, a convention we've mismatched, or a genuine bias? We want to converge on the right
interpretation before it anchors the thesis.
