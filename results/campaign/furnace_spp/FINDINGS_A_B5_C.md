# Findings: A (harden #9), B5 (fig 5.2 ref), C (analytic NEE / mechanism) — 2026-06-26

## A — furnace NEE over-count is invariant to volprim_prb settings (#9 further hardened)
Furnace (albedo 1, white-constant env, single Gaussian, sigma 6, 256 spp, 4 seeds, max_depth 256),
centre over-count by setting:
| setting | centre over-count |
|---|---|
| NEE, exact ellipsoids, bisection (baseline) | **+9.74%** |
| NEE, tessellated ellipsoidsmesh             | **+9.73%** |
| NEE, Newton solver                          | **+9.95%** |
| NEE, hide_emitters=1                         | -35.4% (removes the bounce-0 background; red herring, not the bias) |
| analog (control)                             | **0.000%** |
=> The over-count is **independent of shape (exact vs tessellated) and solver (bisection vs Newton)**;
only flipping use_nee on/off changes it. Combined with the spp-invariance (Phase 0, flat to 16384) and
depth-invariance, the bias is intrinsic to the NEE estimator, not a config artifact. (epanechnikov kernel
run produced no row -- not captured; gaussian is our comparison kernel anyway.)
NOTE: volprim_tomography is absorption-only (no scattering) so it CANNOT render the albedo=1 scattering
furnace -- it is not an alternative Mitsuba mode for #9; volprim_prb is the only scattering integrator.

## B5 — fig 5.2 reference: analytic-truncated beats a Mitsuba render
We CAN set the 3-sigma bound on the Mitsuba side (ellipsoid `extent=3.0`) and render the single-Gaussian
absorption reference via volprim_prb (albedo 0). But that render carries Mitsuba's own reimplementation
offset: corner pixel 0.9937 (should be env=1.0), centre 0.8462 vs ideal 0.853, RMSE vs ours = 1.1e-2 --
i.e. ~0.6% systematic, the SAME "small reimplementation offset" the pair/cloud rungs already show.
The closed-form analytic reference, truncated to the renderer's 3-sigma kernel support, matches ours to
RMSE 2.0e-5. => KEEP the analytic-truncated reference for the single-Gaussian rung (cleanest GT that
isolates our numerical correctness); the Mitsuba-extent-3 render is a worse reference. (abs_single_ref.exr
banked for the record but not wired into the figure.)

## C — analytic NEE transmittance (Jorge #2): both sides already analytic
- OURS: `compute_transmittance_to_env` (sampling.cuh) accumulates total optical depth analytically
  (sum of per-primitive closed-form optical_depth over [entry,exit] + inline anyhit integration) and
  returns exp(-tau). Zero variance. The DISTANCE sampler (argmin free-flight) is used only to place the
  scatter vertex, NOT for NEE transmittance -- exactly Jorge's recommended split.
- MITSUBA: volprim_prb `eval_transmittance` = product of exp(-density_integral*sigma_t) per primitive --
  also analytic. So the NEE bias is NOT a stochastic-transmittance / distance-sampler-as-transmittance
  issue on either side.
- MECHANISM (hypothesis; furnace proves the EFFECT, this is our reading of volprim_prb to be confirmed by
  Jorge): volprim_prb adds TWO env contributions per bounce -- (1) the free-flight CONTINUATION that
  escapes and hits the emitter, MIS-weighted `mis_weight(prev_event_pdf, emitter_pdf)` (volprim_prb.py
  ~L176), and (2) the NEE emitter sample with analytic transmittance, MIS-weighted
  `mis_weight(ds.pdf, nee_pdf)` (~L220). Both estimate the same single-scatter direct term but through
  DIFFERENT measures (free-flight escape vs analytic transmittance); the balance-heuristic weights are
  solid-angle pdfs that do not appear to account for the volumetric free-flight measure, so the two
  strategies' weights need not partition unity -> the direct term is counted with total weight > 1 ->
  over-count that grows with optical thickness and overlap (furnace: +9.7% at sigma6 -> +31% at sigma12).
  Thesis B1 wording ("stochastic shadow transmittance") is therefore wrong and must be corrected to this
  MIS-measure-mismatch description.
