# FINDINGS — validation of the CUDA Gaussian volumetric path tracer

A running, evidence-backed log of what we have *concluded* about the renderer's
correctness, validated against Jorge Condor's Mitsuba `volprim` implementation of
*Don't Splat Your Gaussians* (DSYG). Each entry states a claim, the evidence
(numbers + artifacts), and a status. Newest conclusions first within each section.

**How to maintain this:** when an experiment yields a conclusion, add it here with
the metric and the artifact path. Keep entries terse but quantified. Distinguish
RESOLVED (proven) from OPEN. This file is the source of truth over scratch scripts
and chat logs.

**Headline (2026-06-01):** The renderer's **absorption** path (albedo = 0) is
validated end-to-end, from a single Gaussian up to the full 652-primitive cloud.
The long-standing cloud "dark blob" bug **did not exist** — it was a
wrong-reference artifact. **Scattering (albedo > 0) remains unvalidated** and is
the next campaign.

---

## 0. How to compare correctly (methodology, learned the hard way)

These are preconditions for any CUDA-vs-Mitsuba comparison to be meaningful:

- **Use `volprim_prb`, NOT `volprim_tomography`, as the reference.** `tomography`
  sets `kernel_full_range=True` → it integrates the *infinite* Gaussian line
  integral. Our renderer (and the DSYG paper, §6.1) use the **3σ-truncated chord**
  integral. `prb` (`kernel_full_range=False`) is the matching model. Comparing
  against tomography injects a systematic tail difference (~2e-3 single-Gaussian,
  much larger in the cloud) that is NOT a CUDA bug.
- **Use Mitsuba's analytic `{"type":"ellipsoids"}` shape, NOT `ellipsoidsmesh`.**
  The mesh shell (`uv_sphere`=72 tris, `ico_sphere`=20) gives slightly wrong chord
  endpoints (a per-primitive tessellation error that compounds over many prims).
  The `ellipsoids` shape does exact analytic ray-ellipsoid intersection, like CUDA.
- **Match the convention:** `optical_thickness = sigma_t · sigma_multiplier`, with
  **no** `(2π)^{3/2}·∏scale` bridge (see §1).
- **The radial ring-mean / transmittance-binned diff** is the key tool: it averages
  out MC noise to expose sub-noise *systematic* bias. A zero-mean diff is noise/AA;
  a one-signed diff is a real bias.
- **CUDA's analytic-direct (§5) makes albedo=0 renders near-deterministic**, so
  low spp suffices on the CUDA side; the Mitsuba side is stochastic and needs spp.

Tooling: `tools/refs/render_single_gaussian_via_prb.py`,
`render_two_gaussian_via_prb.py`, `render_cluster_via_prb.py`,
`render_cloud_prb_absorption.py`, comparator `cmp_pair.py`. Mitsuba needs Jorge's
custom build via `tools/refs/with_jorge_mitsuba.sh` + the venv at `tools/refs/.venv`.

---

## 1. Convention & core fixes — RESOLVED

- **Escape double-count (commit `3bb3b93`).** The raygen escape branch multiplied
  by `exp(-τ)` on top of analog free-flight sampling that already bakes it in →
  `exp(-2τ)·env` in expectation. Single Gaussian matched `exp(-2τ)` before, `exp(-τ)`
  after. RESOLVED.
- **PLY-load convention (commit `241d47b`).** Loader applied
  `optical_thickness = sigma_t·mult·(2π)^{3/2}·∏scale`, double-normalising — the
  per-primitive `optical_depth` already mass-normalises via
  `density_norm_factor = (2π)^{-3/2}·∏(1/scale)`. Correct mapping is just
  `optical_thickness = sigma_t·sigma_multiplier`. The scale=1 unit test is BLIND to
  this (the factor collapses into the analytic comparator's M) — must test
  mid-range transmittance. PROVEN: single Gaussian scale=1, σ=4 → CUDA vs Mitsuba
  RMSE 0.0084. RESOLVED.
- **Environment asset (`assets/white_constant.hdr`).** Stored RGBE = 255/256 =
  0.996094, a ~0.4% compounding multiplier on every pixel. Rewrote to exactly 1.0
  (backup `.bak-0996`). RESOLVED.

---

## 2. Single Gaussian (albedo = 0) — RESOLVED, exact

- **Isotropic, scale=1:** CUDA matches the exact 3σ-truncated analytic
  `τ(d)=M/2π·exp(-d²/2)·erf(√(9-d²)/√2)` to **+4e-6** (0/40 radial rings biased),
  unbiased, log-log RMSE-vs-spp slope −0.4999 (ideal MC). Matches `volprim_prb`
  with the analytic shape to **+2.9e-6** (was +2.94e-4 with the tessellated mesh).
- **Truncation model confirmed by the DSYG paper** (§6.1: "bound the kernel's
  support to three times the standard deviation, covering 99.73%"). Our
  `optical_depth` integrates the 3σ-truncated chord — exactly the paper's intended
  model. Do NOT extend `GAUSSIAN_EXTENT_F=3` (it violates the paper and blows up the
  BVH bound volume ~37×).
- **Anisotropic + rotated (whitening transform):** scale=(1,0.5,0.75), 30° about +Z.
  CUDA vs `prb` (analytic shape): mean diff **+1.7e-5** → 0 as Mitsuba spp grows
  (RMSE 1.95e-3 @4096 → 6.8e-4 @32768, ratio 2.87 vs ideal 2.83). Quaternion
  handedness verified visually (same ellipse tilt). Validates the whitening transform
  (`rot·rcp_scale`, normalization `(2π)^{-3/2}·∏(1/s)`), previously only tested at
  isotropic identity. Artifacts: `renders/single_gaussian/SG_transformed_compare*.png`.

---

## 3. Where the single-Gaussian residual variance comes from — EXPLAINED

The CUDA-vs-Mitsuba residual on a converged single Gaussian is **100% Mitsuba's**;
CUDA-analytic-direct is deterministic (per-sample variance ~5e-5, spp-flat).

- Measured two ways that agree: spp-scaling V=0.0152; 8-seed per-pixel variance
  V=0.0153.
- Mitsuba `prb` variance = **0.32 × pure-binary-escape Bernoulli p(1-p)**, with the
  0.32 factor FLAT across the whole transmittance range → variance is `p(1-p)`-shaped
  (peaks at T≈0.5, ~0 at T≈0 and T≈1), uniformly scaled.
- Mechanism (`volprim_prb.py` `sample_segment`): prb is a **partially**
  Rao-Blackwellized free-flight estimator — segment transmittance `exp(-seg_τ)` is
  analytic (folded into throughput), but ONE residual stochastic collision test per
  ray remains. That residual binary decision is the entire variance; the analytic
  folding scales it ~3× below pure binary. Artifact: `SG_variance_anatomy.png`.

---

## 4. Overlap ladder (albedo = 0) — RESOLVED up to the cap

- **2 Gaussians, distinct positions/depths:** mean diff −7.8e-7, no bias in the
  overlap lens. `SG_two_gauss_compare.png`.
- **N=5 cluster:** mean diff +2.1e-6, no interior bias. `SG_cluster_n5_compare.png`.
- **Traits (8 prims: anisotropic + rotated + per-prim varied σ, non-collinear):**
  mean diff −6.6e-6, no bias even in the dense core. ⇒ Mitsuba's high-overlap bias
  (§6) is NOT caused by anisotropy/rotation/varied-σ — only by overlap *depth*.
  `SG_traits_compare.png`.

Scenes: `cluster_validation` (env `SG_CLUSTER_MODE ∈ {n5,stress,traits}`) in
`test/scenes/single_gaussian.cpp`, mirrored exactly in `render_cluster_via_prb.py`.

---

## 5. Analytic-direct variance reduction (`ENABLE_ANALYTIC_DIRECT`) — RESOLVED

- The albedo=0 image was a high-variance analog binary estimator (Bernoulli p(1-p);
  predicted 5.03e-3 = measured 5.01e-3 @8192spp). Replaced the bounce-0 direct term
  with its conditional expectation: `throughput · exp(-τ) · env` computed
  deterministically (Rao-Blackwellization). Unbiased; collapses MC noise on the
  unscattered component (the entire image when albedo=0).
- Result: AD @1 spp (RMSE 9.8e-4) == analog @~71000 spp; ~250× lower RMSE at every
  spp; global bias ~0.
- **Relationship to Mitsuba:** inspired by prb's transmittance folding but goes one
  step further — prb keeps a residual stochastic collision test (§3); AD eliminates
  it entirely for the direct term. Same mean (both unbiased), strictly lower variance.
- **Scope:** only the unscattered/direct term. For albedo>0 the scattered component
  is still stochastic (untested, §7). Only possible because Gaussians have analytic
  transmittance.
- GOTCHA: must pass the CAMERA-ORIGIN inside-set to `compute_transmittance_to_env`,
  not the scatter point's `active_prims_`.

---

## 6. Cap / overlap-depth findings — collinear stress test

Stress scene = K Gaussians collinear in z, total mass FIXED=10 ⇒ by additivity the
image is *identically* a single M=10 Gaussian (closed-form ground truth at any K).

- **FINDING — CUDA `MAX_ACTIVE_PRIMS` cap bug (FIXED).** At K>64, CompactSet
  SILENTLY dropped overlapping prims past the 64th → under-absorption (too bright).
  Center T at K=100→0.362, K=150→0.508 vs correct 0.205, EXACTLY predicted by
  `exp(-min(64,K)/K·τ₀)` (match to 1e-4). Raised `MAX_ACTIVE_PRIMS` 64→**128** and
  kept `HIT_BUFFER_CAPACITY`=128 (the expensive buffer; 256 made renders ~6× slower
  from local-memory blowup). With caps lifted, all K match analytic to 2.3e-4.
  Artifacts: `SG_active_prims_cap_bug.png`, `SG_stress_trend.png`.
  NOTE: the proper fix for pathological overlap is graceful overflow handling, not a
  bigger cap. The cloud's overlap is <64 so this never affected it (see §7).

- **FINDING — Mitsuba `volprim_prb` under-absorbs at high overlap (reference
  limitation).** At K=40/64 (where CUDA = analytic truth), Mitsuba center =
  0.278/0.447 vs correct 0.205, worsening *monotonically* as the same mass fragments
  into more prims (0.278→0.447→0.593→0.702 at K=40/64/100/150). So the prb reference
  itself is biased where many primitives overlap. CAVEAT: collinear stacking is
  degenerate; could be a robustness failure specific to perfectly-stacked ellipsoids
  rather than a general bias. Did NOT affect the cloud (§7).

---

## 7. The cloud — RESOLVED: "dark blob bug" was a wrong-reference artifact

The central open problem of the project. Verdict: **CUDA was correct all along.**

- **Setup:** matched reference `tools/refs/render_cloud_prb_absorption.py` —
  `volprim_prb`, analytic `ellipsoids` shape, albedo = PLY (≈0), sigmat_scale=7.5,
  the same 24 cameras. (The pre-existing `refs_prb_pyr0/` was mis-configured:
  albedo=0.9 scattering, σ=60 — not the absorption validation config.)
- **Result, all 24 cameras:** aggregate mean diff **+1.55e-5** (max per-cam 7.8e-5),
  RMSE 0.0094, interior (T<0.2) bias **+1.5e-4** (positive/tiny — no dark blob).
  vs the historical "bug": RMSE 0.327, mean 0.415 vs 0.555.
  Artifacts: `renders/cloud_success/` (montage + SUMMARY.txt + 24 EXRs/side).
- **Root cause of the phantom bug:** the old comparison used `volprim_tomography`
  (infinite-extent absorption — a different physical model), likely also tessellated
  shells and the pre-`241d47b` convention. Against the correct reference, CUDA matches.
- **Cap-independence proven:** old-caps (64/128) vs lifted (256/256) cloud renders
  are **bit-identical across all 24 cams** (max diff 0.000e+00) ⇒ cloud overlap is
  genuinely <64; the match is real, not an artifact of the cap fix.
- **Silhouette-band diff decomposed** (the thin outline in the diff): at 1024 spp,
  band RMSE 0.0084 = **93% Mitsuba MC noise** (seed-to-seed/√2 = 0.0081, →0 as
  ~1/√spp, slope −0.476) + a **~0.0022 irreducible, zero-mean floor** (mean diff
  +1.5e-4). Artifact: `renders/cloud_lifted/SG_cloud_band_vs_spp.png`.
- **The 0.0022 floor — source still OPEN (an earlier "it's Mitsuba's `:387` error"
  claim was RETRACTED, see below).** Ruled out by test: (a) pixel reconstruction
  filter — forcing Mitsuba to box (matching CUDA's box AA) left the systematic
  identical (gaussian 0.0019, box 0.0020), NOT the filter; (b) sub-pixel registration
  — best sub-pixel shift of CUDA vs Mitsuba is exactly (0,0), any ½-px shift makes it
  3–6× worse, so they are already aligned.
  - **RETRACTED claim:** "CUDA is 100× more accurate at the edge; the floor is prb's
    `:387` full-range approximation." DISPROVEN: a 4-seed (noise-removed) prb render
    on the single Gaussian (exact truth) has edge systematic = **0.00000** (RMSE 0.00202
    == its own noise floor 0.00204). prb is UNBIASED at the single-Gaussian edge; it does
    not sit on the full-range model. The earlier "2e-3" was prb's MC *noise*, not error,
    and the "100×" wrongly compared CUDA's analytic value to that noise. CUDA matches
    truncated truth to 2e-5 (analytic), but prb is equally correct in the mean.
  - **What remains true:** the cloud edge shows a ~0.0022 zero-mean systematic between
    CUDA and converged-prb (seed-decomposed @1024spp; only 2 cloud seeds, so the
    estimate itself is soft). Since the single-Gaussian systematic is ~0, this is NOT a
    single-primitive edge effect — it must arise from overlap/accumulation in the cloud,
    and we CANNOT attribute it to CUDA or prb without an independent third reference
    (no closed form for the cloud). OPEN. To resolve: build a CPU brute-force
    transmittance ray-march of the 652 truncated Gaussians and adjudicate.
  - Artifact: `renders/cloud_lifted/SG_cloud_band_vs_spp.png`. The bulk cloud agreement
    (mean diff 1.5e-5, interior exact) is unaffected by this small edge-only question.
  - **RESOLVED (2026-06-02) — the edge floor is ANTIALIASING, CUDA's transmittance is EXACT.**
    Built an independent double-precision brute-force (BF) transmittance reference
    (`tools/refs/cloud_bruteforce_transmittance.py`): for albedo=0 the image is a pure
    line integral, so BF analytically sums the 3σ-truncated-chord optical depth over all
    652 ellipsoids in float64 (NB: this is "tomography done right" — the truncated kernel,
    unlike the abandoned `volprim_tomography` integrator's infinite kernel). Ablation chain
    (each toggled, measured vs BF, then REVERTED — none was a fix):
      • MC noise → refuted (systematic stable at 0.0019 over 8 seeds).
      • fast-math/transcendental precision → refuted (precision build bit-identical).
      • camera fractional-scale (J) → refuted (best affine warp = scale 1.000, shift 0).
      • OptiX-entry vs full analytic quadratic (F) → refuted (bit-identical render).
      • segment-march vs direct additive full-chord sum → refuted (bit-identical).
      • float32 vs double optical_depth → refuted (bit-identical).
      • DECISIVE: disabling sub-pixel jitter (point-sample center, 1spp) → CUDA vs BF edge
        RMSE collapses 0.00199 → **0.00002**. So CUDA's transmittance is EXACT; the 0.002
        was box-filter antialiasing (⟨exp(−τ)⟩ over the pixel) vs BF's center point-sample.
    CONCLUSION: no bug in CUDA or Mitsuba; the edge residual is purely pixel-reconstruction (AA).
  - **AA fully decomposed (2026-06-02) via a box-AA'd brute-force** (SS=4 supersampled, 16
    sub-samples/px = box filter; tools/refs/cloud_bruteforce_transmittance.py SS env). Edge band:
      • the box-AA *effect* itself (box-AA vs point-sample truth) is only **0.00065** — tiny.
      • CUDA 64spp vs box-AA-truth = 0.00188; CUDA 1024spp vs box-AA-truth = 0.00048; ratio
        **3.95 ≈ 4.0 = pure 1/√spp**. ⇒ CUDA's AA is the CORRECT box reconstruction; the 0.0019
        was CUDA's own residual SUB-PIXEL JITTER MC NOISE at 64spp (transmittance is deterministic
        per ray, but the random jitter over 64 samples leaves edge noise — reducible, not a bug).
      • Mitsuba(8-seed) vs box-AA-truth systematic = 0.00062 = the box(CUDA)-vs-gaussian(Mitsuba)
        reconstruction-filter difference. Both AA schemes are valid.
    So the "0.002 floor" = ~0.0019 CUDA AA jitter noise (→0 as 1/√spp) + ~0.0006 box-vs-gaussian
    filter. This reconciles the earlier puzzles ("Mitsuba closer to point than CUDA box-AA": CUDA's
    64spp noise swamped the 0.0006 filter effect; "Mitsuba box≈gaussian systematic": the systematic
    was dominated by CUDA jitter noise, not the filter). CORRECTION: analytic-direct is noise-free
    in the flat interior/background but the SILHOUETTE still carries AA jitter noise — earlier
    "albedo=0 image is noise-free" was wrong at the edge.
  - Benign / edge-only / does not touch the validated result. All ablation edits reverted; tree
    holds only the cap fix + cluster scenes + tools/refs scripts + FINDINGS.
- **Why the cloud looks near-black:** it's a pure absorber (albedo≈0), so each pixel
  is `exp(-τ)·background`; at σ=7.5 the interior is optically thick → black silhouette
  with soft edges. Jorge's nuanced `refs_pyr0` are **scattering** renders (interior
  lit by multiple scattering) — a different regime (§7 open items / §8).

---

## 8. Known limitations & OPEN items

- **Scattering (albedo > 0) is UNVALIDATED.** The ADT argmin scatter sampling +
  NEE/MIS/HG path has never been checked against Mitsuba. This is the next campaign:
  climb the same ladder (single → cluster → cloud) at albedo>0 vs `volprim_prb`
  (albedo 0.9). It is also how we reproduce Jorge's nuanced cloud *look*.
- **Low-σ interior absorption check (cheap, recommended next).** Current cloud
  interior agreement is partly trivial because both renders clamp to black there.
  Re-render at lower σ (≈1.5–2.5) so the interior is gray → a non-saturated interior
  transmittance comparison.
- **uint16 spp ceiling.** `Image::sample_counts_` is `uint16_t` → Welford count wraps
  at 65536 spp; RMSE floors there. Fix = widen to uint32. Not yet done; renders
  ≤65535 spp are valid.
- **Cap overflow is silent.** `MAX_ACTIVE_PRIMS`/`HIT_BUFFER_CAPACITY` drop excess on
  overflow without warning. Current values (128/128) cover the cloud; denser scenes
  need graceful handling or per-scene tuning (accepting the perf cost).
- **AA-floor attribution** (§7) could be nailed by forcing identical pixel filters
  (box on both) or rendering at 2× and downsampling.

---

## 9. Pointers to older notes

- `tools/refs/CONCLUSIONS.md` — the voxel-reference pipeline (stalled; spatial
  convention unknowns). Superseded by the prb-reference approach above.
- `REGRESSION_ANALYSIS.md` — why the April-26 "wispy" render was buggy-but-flattering,
  not a regression.
- `claude-docs/` — older planning notes (denoiser, NEE, quality roadmap, hybrid
  wavefront, cloud-calibration). Historical; verify against current code before use.
