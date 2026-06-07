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

## 8. Scattering validation (albedo > 0) — IN PROGRESS

The scattering campaign. Tools: `SG_ALBEDO` env var on both
`test/scenes/single_gaussian.cpp` (CUDA) and
`tools/refs/render_single_gaussian_via_prb.py` (Mitsuba). The Mitsuba script also
gained `SG_NEE` (default 1), `SG_MAX_DEPTH` (default 32), `SG_SIGMA` (default 4).

### 8.0 How to read these RMSE numbers (CRITICAL)
RMSE between two **independent stochastic** renders is NOT a disagreement metric — it
is dominated by Monte-Carlo noise. Decompose it: `RMSE² = systematic² + noise²`, where
`systematic = |mean(diff)|` (the real, irreducible error) and `noise = std(diff)`
(zero-mean, → 0 as 1/√spp). Measured across every albedo=0.9 rung (script: `/tmp/decomp.py`,
mirror of `cmp_scatter.py`):

| rung | RMSE | systematic | noise | systematic / RMSE |
|---|---|---|---|---|
| single Gaussian σ=4 | 0.00197 | **0.00000** | 0.00197 | 0.17% |
| single dense σ=12 | 0.00274 | **0.00003** | 0.00274 | 0.99% |
| n5 cluster | 0.00201 | **0.00000** | 0.00201 | 0.18% |
| traits cluster | 0.00138 | **0.00004** | 0.00138 | 2.82% |

⇒ RMSE is **~100% noise**; the real systematic is ≤ 4×10⁻⁵ everywhere (essentially exact).
RMSE ~0.002 at 16k spp is the **noise floor**, not an error floor — two correct MC renders
cannot beat `√(var_A+var_B)` at finite spp; it shrinks as 1/√spp (proven: traits RMSE
0.00273→0.00138 with more spp while its systematic held flat). So judge correctness by the
**systematic / per-bin / regional** numbers (which cancel noise), NOT by RMSE. RMSE actually
*hides* the one real signal: traits' +0.0002 core residual sits ~35× below its own noise
(§8.3). PSNR (10·log₁₀(1/MSE), peak=1): single≈54.1 dB, dense≈51.2 dB, n5≈53.9 dB,
traits≈57.2 dB — all noise-floor-limited, so PSNR carries the same caveat as RMSE.

### 8.1 Furnace test (reference-free energy-conservation invariant)
A conservative medium (albedo = 1) embedded in a **constant** radiance field must
reproduce that field exactly: `L = L_env` everywhere (RTE: dL/ds = −σ_t·L_env +
σ_t·L_env = 0). The Gaussian must be **invisible** against the background. This is
analytic and needs **no reference renderer** — it is a pure check on the entire
scatter + NEE + multi-bounce machinery. Independent of kernel truncation, geometry,
or bounce count.

- **CUDA PASSES.** Single Gaussian, albedo=1, σ=4, constant env=1.0:
  image mean = **1.00001** at 4096 spp; residual scales as MC noise (±12% at 256 spp
  → ±3.3% at 4096 spp ≈ exact 1/√spp), image-mean SEM ≈ 6e-5 ⇒ **no detectable energy
  bias**. CUDA's NEE path is energy-conserving.
- **Mitsuba `volprim_prb` NEE path FAILS the furnace test by +6.5%.** With
  `use_nee=True` the Gaussian center is rendered +6.5% **brighter** than background
  (in-scatter overcounted); **identical at max_depth 32 and 256** (not truncation).
  With `use_nee=False` (pure analog) Mitsuba is **exactly 1.00000 everywhere**.
  ⇒ Mitsuba's NEE/MIS combination is not energy-conserving here; its **analog
  (NEE-off) path is exact** and is the only trustworthy ground truth for scatter.
  **CONSEQUENCE: all scatter references must be rendered with `SG_NEE=0`.**

### 8.2 Rung 1 — single Gaussian, albedo = 0.9 (PASS)
CUDA (NEE on, furnace-verified) vs Mitsuba **analog** (`SG_NEE=0`), both σ=4,
albedo=0.9, analytic `ellipsoids` shape, **box** reconstruction filter, 16384 spp:
- mean diff = **−0.00000**, RMSE = **0.00197**, maxabs = 0.0144
- the RMSE equals the absorption noise floor (§7: box-AA jitter), **not** bias
- **per-radiance-bin diff ≤ ±0.00006 across all 8 bins** — zero systematic structure
- central densest region (most multiple-scatter): diff = +0.0000 at 96×96
⇒ ADT argmin scatter sampling, phase function, albedo weighting, NEE, and the
multi-bounce loop produce the correct converged image for a single Gaussian.

### 8.3 Rung 2 — clusters, albedo = 0.9 (PASS, with one isolated overlap residual)
All vs Mitsuba **analog** (`SG_NEE=0`), `ellipsoids`, box filter. `SG_ALBEDO`/`SG_NEE`/
`SG_MAX_DEPTH`/`SG_SIGMA` env knobs added to the cluster + single-Gaussian scripts.

- **n5 cluster (5 isotropic overlapping, M=2): PASS.** 16384 spp: mean diff −0.00000,
  RMSE 0.00201, all 8 radiance bins ≤ ±0.00016, central 96×96 = −0.00005. Clean.
- **traits cluster (8 anisotropic+rotated+varied-σ, overlap ~8): PASS with a small,
  real, isolated residual.** CUDA furnace on this dense cluster (albedo=1) = mean
  1.00003, flat — **CUDA conserves energy in dense overlap**. At albedo=0.9 vs
  analog: RMSE 0.00138 (65535/49152 spp), mean +0.00004, but the **dense core is
  systematically CUDA-brighter by ~+0.0002** (central 96×96 +0.00019 @ ~6 SEM; very
  center 8×8 +0.0012; deepest radiance bin +0.0005).

**Diagnosis of the traits core residual (each step a controlled experiment):**
1. **Not energy** — CUDA furnace on traits is flat (mean 1.00003).
2. **Half was Mitsuba max_depth=32 truncation.** Raising Mitsuba 32→128 (0.9¹²⁸≈1e-6,
   so 128 ≈ unbounded) cut the core diff in half (96×96 +0.00035→+0.00017). Both
   sides now run depth 128.
3. **The remaining ~+0.0002 is NOT convergence.** CUDA 65535 / Mitsuba 49152 (both
   d128): overall RMSE halved 0.00273→0.00138 (noise) but the **core systematic held
   flat** (96×96 +0.00017→+0.00019). Convergence-stable ⇒ a genuine bias.
4. **It is OVERLAP-specific, not density.** Single dense Gaussian σ=12 (deep core
   T~0.85, heavy multiple-scatter, **no overlap**), albedo=0.9 vs analog d128:
   mean −0.00003, all bins ≤±0.0002 with sign flips = pure noise, **zero systematic**.
   Single σ=4 likewise clean (§8.2). So multiple-scatter depth alone does not trigger
   it; simultaneous overlap does.
5. **It is NOT the albedo blend.** All traits prims share albedo=0.9, so the
   σ-weighted `evaluate_albedo` returns exactly 0.9 regardless of weights.
6. **It is NOT the entering-ray transmittance.** Absorption traits (albedo=0) passed
   exactly (mean −6.6e-6), so `compute_transmittance_to_env` is correct for rays
   entering the overlap from outside.

⇒ **Residual is localized to the scattering-specific overlap path** — most likely the
NEE shadow-ray transmittance evaluated **from a scatter vertex INSIDE overlapping
primitives** (uses `exit_from_inside` on several prims — a distinct path from camera
rays entering from outside), or the ADT argmin scatter-distance distribution under
overlap. CUDA-brighter sign ⇒ slight *over*-estimate of in-scattered light in overlap.
Magnitude is tiny (0.03% core, ≤0.16% peak) but **convergence-stable and real**, and
the cloud (overlap 37–45 ≫ traits' ~8) may compound it — so it matters for the cloud.
**OPEN.** Candidate fix sites: `compute_transmittance_to_env` interior-start branch
(sampling.cuh), `sample_scattering_event` argmin under overlap.

Artifacts: `test_results/scatter/` (cuda_*), `test_results/single_gauss/mitsuba_*`.

### 8.4 Cloud scattering — FULL RUNG PASS (cam 0)
First scattering render of the full 652-Gaussian cloud. CUDA `cloud_asset_scattering`
(albedo=0.9 default) vs Mitsuba `volprim_prb` **analog** (`SG_NEE=0`), both σ=7.5
(Jorge's confirmed LINEAR cloud density scale — see [[reference_asset_density_scales]]),
albedo=0.9, max_depth=128, box filter, cam_0000, 800×800. Tools: `SG_CAM` selector on
the CUDA cloud scene (single-camera, 0000.exr); `SG_ALBEDO`/`SG_SIGMA`/`SG_MAX_DEPTH`
on `render_cloud_prb_absorption.py` (ALBEDO>0 → `refs_prb_scattering/`).
Resumable study: `tools/refs/cloud_scatter_study.sh` + `cloud_scatter_metrics.py`.

**spp ladder (cross CUDA vs Mitsuba, same spp):**
| spp | RMSE | PSNR | systematic | t_CUDA | t_Mitsuba |
|----:|-----:|-----:|-----:|------:|------:|
| 64 | 0.05976 | 24.5 | 0.00007 | 54s | 33s |
| 128 | 0.04218 | 27.5 | 0.00000 | 114s | 65s |
| 256 | 0.02980 | 30.5 | 0.00003 | 239s | 128s |
| 512 | 0.02108 | 33.5 | 0.00001 | 479s | 253s |
| 1024 | 0.01491 | 36.5 | 0.00004 | 956s | 496s |

**The RMSE is 100% noise; the systematic error is below detection (~10⁻⁴).** Proven
(no code) via a converged reference M* = mean of 9 Mitsuba seeds @512 (eff ~4600,
residual noise 0.00358). CUDA(spp) vs M* fits `RMSE² = 0.1688/spp + 0.000013`:
- slope ⇒ CUDA noise const **kC = 0.411** (matches independent self-conv estimate);
- intercept floor (0.00354) **equals M*'s own noise (0.00358)** ⇒ systematic²
  = floor² − M*noise² = 0 ⇒ **systematic ≈ 0** (regional means −4e-5; σ=16 blurred
  diff flat, peak 0.001). Cross-RMSE tracks 1/√spp exactly (0.0598→0.0149 over 16×spp
  = ratio 4.01 = √16) ⇒ **no error floor**.
- **DIRECT MEASUREMENT (added: `--seed` flag + 16-seed CUDA* vs 8-seed M*, noise-free
  diff — no extrapolation).** Each renderer's residual noise measured from systematic-
  free seed-pair diffs (CUDA* 0.00454, M* 0.00380). Tool: `cloud_systematic_direct.py`.
  - **global systematic = −1.6e-5 ± 8e-6** (2σ) — essentially zero, ~5 decimal places.
  - Spatial structure seen at 8 seeds SHRANK at 16 (central 96² −1.1e-4→−2.1e-5;
    192² −1.6e-4→−6.1e-5) ⇒ it was noise (a real bias would hold; noise shrinks √N).
  - **BUT a real residual confirmed in the densest core:** with an INDEPENDENT mask
    (dense defined from a separate render, so selection can't bias the noise) the broad
    body (T<0.6) = −1.2e-5 [1.0σ]=0, but the deepest pixels **T<0.4 = +1.0e-4 [3.3σ]**,
    CUDA-brighter — same sign as the §8.3 traits overlap residual (+0.0002), ~2× smaller,
    confined to the most-overlapped pixels. (A self-mask gave a spurious +1.05e-4 "8.4σ"
    = select-by-noisy-reference bias — do NOT mask by the noisy variable being compared.)
  ⇒ So at cloud scale: agreement to ~10⁻⁵ globally; the overlap-scatter residual (§8.3)
    DOES echo here as a tiny **+1e-4 in the densest core only**. RMSE is ~150–1000× the
    true disagreement (0.015 vs 1e-5–1e-4) ⇒ overwhelmingly noise, now MEASURED not bounded.
- Decomposition method (how noise vs bias is separated): noise ∝ 1/spp & zero-mean
  (cancels under spp↑ and pixel/seed averaging); bias is spp-independent & survives
  averaging. So `RMSE²(spp) = k²/spp + ⟨S²⟩`: slope=noise, intercept=systematic. Valid
  ONLY because the estimator is unbiased MC+RR with NO denoise/clamp. Artifacts:
  `renders/cloud_converge/cloud_systematic_proof.png`, `test_results/cloud_*_bundle/`.

⇒ **CUDA cloud scattering is VALIDATED: same physics as Mitsuba to ~10⁻⁵ globally**
(measured), with one tiny real residual (+1e-4 in the densest-overlap core = the §8.3
effect echoing at cloud scale). No error floor; RMSE is ~99.9% noise. Remaining: more
cameras (cam 0 only so far); the nuanced cloud *look* (albedo/σ choice).

### 8.5 SPEED (cloud scattering, cam 0) — CUDA currently slower; algorithmic, fixable
Equal-spp: CUDA **1.93×** slower (throughput 0.934 vs 0.484 s/spp). Equal-QUALITY (the
fair metric): noise consts kC=0.411 vs kM=0.243 ⇒ CUDA **2.85× noisier per sample** ⇒
equal-quality slowdown = 1.93 × 2.85 = **~5.5×** (to noise 0.01: CUDA ~1577s vs ~287s).
ROOT CAUSE: Mitsuba `volprim_prb` is partially Rao-Blackwellized at EVERY bounce (folds
analytic segment transmittance into throughput each step, β*=seg_tr); CUDA only does
this at bounce 0 (`ENABLE_ANALYTIC_DIRECT`). That single difference = the entire 2.85×
variance gap. Levers (correctness-preserving, never profiled): (a) **per-step RB** —
extend analytic throughput-folding to all bounces (closes the 2.85× variance gap, lowers
RMSE AND closes most of the speed gap); (b) **throughput** — NSight pass (128-deep
HIT/Event per-ray buffers likely hurt occupancy). NB Mitsuba's *faster* NEE mode is
energy-broken (+6.5% furnace, §8.1) so we benchmark its slower analog; CUDA NEE is correct.

### 8.6 Env-map orientation parity (WS0 — feature-validation prerequisite)
The scattering ladder (§8.0–8.5) ran under a **constant white** env. To validate the real
HDR (meadow) path, CUDA and Mitsuba must sample the **same env direction → pixel**, else any
meadow comparison is meaningless. CUDA's equirect convention (`device/params/environment_map.h`,
`device/core/sampling.cuh` env_is): `u = atan2(z,x)/(2π)+0.5`, `v = acos(y)/π`. Mitsuba's
`envmap` uses `atan2(x,−z)` azimuth → differs by a 90° rotation about +Y (derived analytically,
then confirmed empirically).
- **Method:** perspective camera (ortho collapses the background to one direction — useless),
  near-transparent Gaussian (σ→0) so the background env dominates; render the env directly on
  both sides; sweep Mitsuba `envmap to_world = rotate(+Y, β)`.
- **Azimuth:** β = **90°**. Sharp minimum: RGB RMSE(CUDA vs Mitsuba) = **0.014 at 90°** vs 0.136
  symmetric at ±2° and 0.42 at the next 90° bucket. (A 90° azimuth offset is a convention choice,
  not a bug — the env can be spun freely about the vertical axis; we adopt CUDA's convention and
  rotate Mitsuba to match.)
- **⚠ VERTICAL FLIP BUG (found+fixed).** The β-sweep above only used a **+Z-facing** camera,
  whose view directions lie near the **equator (y=0)** — the fixed line of a y-flip — so it
  could NOT detect a vertical flip, and initially (wrongly) reported "no flip". A proper
  **full-sphere** probe (center pixel = env(exact axis), immune to camera image-flips; CUDA
  knob `SG_VIEW`∈{±x,±y,±z}) found all four equatorial axes match (d≤0.003) but the **poles
  were swapped**: CUDA(+Y)=ground [0.065,0.084,0.026] = Mitsuba(−Y), CUDA(−Y)=sky
  [0.086,0.162,0.313] = Mitsuba(+Y). Since `to_world=rotate(+Y,·)` can't touch the poles,
  Mitsuba's native +Y=sky is physically correct ⇒ **CUDA was rendering the env upside-down.**
  - **Root cause:** `src/thesis/host/utils/io/hdr.cpp` `flip_vertical=true` (kept only to
    "preserve prior stb-with-flip behavior") put the file's bottom (ground) at texture row 0,
    while `env_map.sample` maps y=+1→v=0→row 0, which should be sky. Fixed → `flip_vertical=false`
    (corrects the texture lookup, the `buildCdf` sin θ weighting, and `env_is` consistently — they
    all read the same buffer).
  - **Why no prior test caught it:** constant env (uniform), equatorial +Z camera (flip's fixed
    line), and the isotropic single Gaussian whose in-scatter integral is orientation-invariant
    (§8.7). The **structured cloud under a real directional HDR** is the first test that can see
    it — WS1's cloud rung (§8.8) is exactly what surfaced it.
  - **After fix:** all 6 axes match to ≤0.002 (noise floor). Full-sphere env parity achieved.
- **`env_is` convention check (analytic):** `env_is::sample` reconstructs directions as the
  exact inverse of `env_is::pdf` / `env_map.sample` (azimuth `2π·u_norm−π ↔ atan2(z,x)`, polar
  `π·v_norm ↔ acos(y)`); the solid-angle Jacobian `2π²·sinθ` normalizes the pdf to integrate
  to 1 over the sphere. Minor approximation: `sample` returns texel-**center** directions (no
  within-texel jitter) → sub-texel angular bias, negligible at 4k; WS1's NEE-vs-analog match
  is the empirical confirmation of `env_is` on a real HDR.
- **Baked in:** `tools/refs/render_single_gaussian_via_prb.py` `SG_ENV_ROTY` defaults to 90.
  CUDA knobs: `SG_ENV=meadow`, `SG_PERSP=1`, `SG_FOV`. Artifact:
  `test_results/single_gauss/WS0_orientation_calib.png`.

### 8.7 Real HDR env (meadow) — single Gaussian (WS1, PASS)
First validation under a **real** environment map (4k meadow), exercising the env path +
`env_is` importance sampler (its first real exercise — degenerates to uniform on a constant
env) + NEE under real lighting. Config: single isotropic Gaussian, σ_t=4 (3σ footprint fills
the ortho frame), albedo=0.9, meadow env (roty90, §8.6). CUDA runs NEE-on (default build);
the reference is Mitsuba **analog** (`use_nee=False`) — so this directly tests whether CUDA's
NEE+`env_is` converges to the unbiased analog image.
- **Result (24 CUDA seeds vs 23 Mitsuba seeds, 2048 spp each):** global systematic
  **+0.00011 ± 0.00082 → 0.1σ → statistically zero**; per-channel R/G/B all ≤0.1σ. CUDA\*
  mean 0.09367 vs Mitsuba\* 0.09355 (agree to 0.13%).
- **Firefly note:** the meadow sun makes single-seed renders heavy-tailed (NEE p99.9≈7/max≈140
  vs analog p99.9≈16/max≈86 — the NEE-vs-analog finite-sample tail difference). A single-seed
  1024-spp diff reads +2–3%, *entirely* finite-sample noise — it averages to zero over seeds.
  Robust cross-checks (median|diff|=6e-4; clipped means) confirm no systematic. The multi-seed
  diff-of-means is the unbiased estimator and is the number to trust.
- **Tooling:** `tools/refs/sg_systematic.py` (general multi-seed diff-of-averages, honest SEM
  from seed-pair stds — inflates correctly under fireflies). Seeds in `renders/sg_meadow/`.
- **Conclusion:** `env_is` is correct on a real HDR; the no-within-texel-jitter approximation
  (§8.6) is empirically negligible at 4k.
- **Orientation-blind (important caveat).** This single-Gaussian result was obtained with an
  equatorial +Z ortho camera AND an isotropic scatterer, so it is **invariant to the §8.6
  vertical-flip bug** — it passed identically before and after the fix. It validates `env_is`
  magnitude/normalization and NEE, NOT vertical env orientation. Orientation is validated
  separately (§8.6 full-sphere probe) and at the cloud (§8.8).

### 8.8 Real HDR env (meadow) — full cloud (WS1 cloud rung) — PASS (orientation bug fixed)
The cloud is the first test sensitive to env **orientation** (652 spatially-distinct primitives ⇒
the directional light distribution matters). Comparing the fixed CUDA build vs Mitsuba-analog,
cam 0, σ_t=7.5, albedo=0.9.
- **It caught the §8.6 vertical-flip bug.** Pre-fix 8-seed systematic: global **−0.0109 (7.4σ)**,
  green worst (−0.016); localized to the **background** (CUDA bg [0.082,0.103,0.027] vs Mitsuba
  [0.158,0.193,0.103] — CUDA sampling ground where Mitsuba sampled sky). This is the failure that
  drove the full-sphere probe and the fix.
- **Post-fix (8 CUDA seeds vs 8 Mitsuba seeds, 256 spp):** backgrounds match exactly;
  **median|diff| = 0.0011** (45× better than pre-fix 0.0496 — the bulk image matches to 0.1%);
  global systematic **−0.0029 ± 0.0013 (2.2σ)**, per-channel R/G/B all ≤1.5σ. The blurred diff
  map (`renders/cloud_meadow_FIXED_diff.png`) is salt-and-pepper red/blue over the body + silhouette
  — **no coherent colored region**, i.e. firefly + sub-pixel edge noise, not a structural bias.
- **The −0.0029 is firefly-limited, not a confirmed bias.** Mitsuba's analog has a heavy firefly
  tail (top-1% body pixels avg 11.6 vs CUDA 0.55 — deep scatter paths hitting the meadow sun) that
  8×256 spp has not fully converged, so the global *mean* is pulled around while the *median* is
  flat. The precise env_is number comes from the firefly-light single Gaussian (§8.7, ≤1e-4); the
  cloud's role is to confirm **orientation + scaling + no structural bias**, all of which hold.
- **VERDICT: PASS.** Orientation bug fixed; cloud-meadow bulk matches Mitsuba-analog; residual is
  MC noise. (Tightening the global mean to ≤1e-4 would need more seeds/spp — optional, not required
  for the claim; `cloud_meadow_seeds.sh` is idempotent so it can be extended.)

### 8.9 HG anisotropy g≠0 (WS2) — sign bug found+fixed, then PASS
Validating forward scattering (g=0.85) vs Mitsuba's `hg` phase function. HG is **invisible under
a constant env** (the phase integrates out for single scatter — same blindness class as §8.6),
so the test uses the meadow. Single Gaussian, σ_t=4, albedo=0.9, CUDA(NEE) vs Mitsuba-analog.
- **Furnace-HG gate (energy):** g=0.85, albedo=1, constant env → flat (bias 5e-5). Forward
  scattering is energy-conserving in CUDA (holds before and after the fix below).
- **⚠ HG SIGN BUG (found+fixed).** Same-sign CUDA(g=0.85) vs Mitsuba(g=0.85) gave global
  **−0.0081 (18.7σ)**, CUDA ~17% dimmer — a genuine bias (clipped≈unclipped, no firefly
  confound). Sign-flip test was decisive: **CUDA(g=+0.85) ≡ Mitsuba(g=−0.85)** (global −0.0007,
  1.4σ; median 0.00026). So CUDA's HG anisotropy entered with the **opposite sign** to Mitsuba's
  (and the standard PBRT/Mitsuba convention where g>0 = forward).
  - **Root cause:** all call sites pass `wi = ray.direction_` (incoming propagation) to
    `phase::sample`/`eval`; with that convention, matching Mitsuba requires the formula to use
    **−HG_G**. Both the NEE (`raygen.cuh`) and continuation (`sample_scattering_event`,
    `sampling.cuh:417`) use the *same* `wi`, so one internal sign flip fixes both consistently.
  - **Fix:** `device/core/sampling.cuh` phase namespace — `g_eff = −consts::HG_G` in `eval_hg`
    and `sample` (keeps sample↔eval↔pdf consistent; furnace still passes). Now user-facing HG_G
    follows the standard convention (HG_G>0 = forward, what clouds need).
- **Post-fix (24 seeds, same sign HG_G=0.85 vs g=0.85):** median|diff| **0.00024**; **clipped(<2)
  global diff = −3e-5** (resolved image matches to 3e-5 once the firefly tail is removed). Global
  +0.0012 (2.4σ) is a small firefly-tail variance difference (forward NEE peak toward the sun),
  not an image bias. **PASS.** Seeds in `renders/sg_meadow_hg085/`.
- **Continuation/multiple-scatter** uses the same fixed phase call; validated end-to-end at the
  cloud in WS4 (money shot). NB the only g≠0 test before this session was none — HG had never
  been exercised, so the sign bug was latent.

### 8.10 MIS (WS3) — phase::eval sign bug found+fixed, then PASS
`ENABLE_MIS` combines phase-IS (Strategy A) and env-IS (Strategy B) at scatter vertices. It is
the **first real consumer of `phase::eval` and `env_is::sample`** — phase-IS NEE and the
continuation both use `phase::sample` only (where the phase value cancels via `phase/pdf=1`), so
`phase::eval` had never been exercised. It's also the path the plan flagged as highest-risk.
- **Initial: BROKEN.** Furnace-MIS −6.8%; meadow CUDA(MIS+HG) vs Mitsuba-analog **−52% too dark**
  (−0.0252, 97σ). Tell-tale: CUDA-MIS noise ~0.0005 vs ~0.13 without — the sampler reduces variance
  ~250× but converged to a *biased* mean ⇒ a weight/normalization error, not sampling.
- **Root cause (`phase::eval` ⇄ `phase::sample` sign mismatch).** Isolating the two strategies
  (forcing w_a=0,w_b=1) showed **env-IS alone is biased −23% even on a uniform white env**, identical
  across R/G/B ⇒ data-independent estimator-math bug. env-IS is the unbiased estimator of
  ∫phase·env·T *only if `phase::eval` equals the distribution `phase::sample` actually draws*. They
  don't match: the PBRT-style sampling inversion and `eval_hg` encode cosθ with **opposite signs**
  of the 2g·cosθ term. Empirically `phase::sample(g_eff=−HG_G)` draws cosθ with mean **+HG_G**
  (forward, validated §8.9) while `eval_hg(g_eff=−HG_G)` peaks at cosθ=**−1** (backward) — verified
  in Python: `sample(−HG_G)` matches `eval_hg(+HG_G)` (RMS log-ratio 0.056) and is the mirror of
  `eval_hg(−HG_G)` (2.53). So env-IS scattered NEE light per a *backward* phase → lost energy. The
  WS2 `g_eff=−HG_G` `replace_all` had wrongly flipped `eval_hg` too (it was originally correct).
- **Fix:** `eval_hg` uses **+consts::HG_G**; `sample` keeps **−consts::HG_G** (`device/core/sampling.cuh`,
  with a comment on the opposite-convention requirement).
- **Post-fix: PASS.** Furnace-MIS exact (mean 1.00011 at σ=4, also PASS at σ=6). MIS == the validated
  phase-IS NEE estimator (CUDA-internal) to **+0.0003 (0.7σ)** — same image, **159× lower variance**.
  vs Mitsuba-analog: +0.0015 (+3%), the *same* offset phase-IS NEE shows (§8.9) = Mitsuba-analog's
  under-converged firefly tail, not MIS. Seeds: `renders/sg_meadow_mis/`.
- **`ENABLE_MIS=true`** (validated; the big variance win for env-map scenes). Note env_is returns
  texel-center directions (no within-texel jitter) — fine at 4k; jitter was tried and reverted (it
  added a pole-division NaN hazard on coarse envs without changing the bias, which was the sign bug).

### 8.11 Money shot (WS4) — cloud + meadow + HG g=0.85 + MIS — PASS, and match-and-beat
The combined "nuanced look" (cloud, real HDR env, forward scattering, MIS) validated vs Mitsuba in
one render (cam 0, σ_t=7.5, albedo=0.9, g=0.85, 256 spp). Artifacts: `renders/bundle_2026-06-04/`
(`RESULTS.md`, `figures/`). **CUDA-MIS is the validated-correct reference here** (its energy is the
furnace/NEE-validated value from §8.1/§8.10).
- **Energy / correctness (mean radiance):** CUDA-MIS **0.3215** vs Mitsuba-analog **0.3242 (+0.9%)**
  — match (the +0.9% is Mitsuba-analog's unconverged firefly tail, same story as §8.8/§8.10, not a
  CUDA bias). Mitsuba-**NEE** = 0.8200 (**+155%**) — its NEE is grossly over-bright in this
  scattering+meadow scene, the amplified cousin of its +6.5% furnace failure (§8.1). So **analog is
  the only trustworthy Mitsuba mode**; its NEE/MIS is not ground truth.
- **Firefly proof (single frame):** CUDA-MIS max **3.1**, p99.9 2.18, **0%** pixels >5. Mitsuba-analog
  max **317.9**, p99.9 57.13, **0.56%** >5. Mitsuba-NEE max 7.8, 0.96% >5. CUDA-MIS converges
  **firefly-free**; Mitsuba spikes the same energy.
- **Equal-quality speed** (cost = k·t, k = noise const, t = s/spp; lower = better):
  CUDA-MIS k=2.255, t=1.808 → cost **4.08**. Mitsuba-analog k=4108, t=0.625 → cost 2568 ⇒
  **CUDA ~630× faster vs Mitsuba's UNBIASED path**. Mitsuba-NEE/MIS k=3.04, t=2.01 → cost 6.10 ⇒
  **CUDA ~1.5× faster vs Mitsuba's BIASED path** (and CUDA is the correct one). Heavy-tailed
  fireflies converge slower than 1/√spp, so k·t *understates* CUDA's edge vs analog.
- **Honest framing (scene-dependent).** This is the env-map regime where MIS + analytic-direct +
  clean sampling win big. On the *constant-env absorption* cloud (§8.5, no MIS benefit) CUDA is
  ~5.5× *slower* per equal quality. The durable per-spp gap (CUDA ~2.85× noisier, §8.5) is the
  target of the Rao-Blackwellization work (TODO A1); MIS already closes the env-scene case.

### 8.12 Path-control robustness sweeps (P1) — PASS, all knobs unbiased
Proving the termination knobs change only variance/speed, **not** the converged mean (invariance =
unbiasedness). All three are compile-time `constexpr` → each value is a rebuild. Metric:
multi-seed `sg_systematic.py`, config vs baseline (same build otherwise). Renders in `renders/p1/`.
- **single Gaussian** (albedo 0.99 → deep multiple scattering, 4 seeds × 4096 spp; SEM ≈ 7e-6):
  every swept value lands at **|Δ| ≤ 1e-6, 0.0–0.1σ**:
  - `RR_DEPTH` {off(9999), 1, 5*, 10} → 0.0σ ⇒ Russian roulette **unbiased**.
  - `MAX_BOUNCES` {32, 64, 128*, 256} → 0.0σ.
  - `MIN_THROUGHPUT` {1e-3, 1e-4*, 1e-5, 0} → <1e-6 ⇒ the throughput cull adds **no measurable bias**.
- **cloud cam 0** (albedo 0.9, σ_t=7.5, 256 spp — the depth-stressing scene): `RR` {off/1/10} and
  `MAX_BOUNCES` {64/256} all agree with baseline to **≤1.3e-5**; **`MAX_BOUNCES=32` darkens by
  −0.00109 (~100× the noise floor)**. This truncation is the *reassuring* result: it shows the test
  can **detect** under-bouncing (deep multiple scattering cut off in dense media), so the "no bias"
  verdicts above are sensitivity-confirmed, not blindness — and the shipped default **128 is
  converged** (64/256 agree to noise; the single Gaussian's short mean path means 32 already
  suffices *there*, but the dense cloud needs ≥64).
- (*) = committed baseline. Caveat: cloud rows have 1 config seed vs 3 baseline seeds so `sg_systematic`
  reports `nan` SEM; the magnitude (MB32 ~100× everything else) is unambiguous regardless.

### 8.13 Coverage: more cameras + low-σ interior (P2) — DONE (view-independent PASS; low-σ surfaces a tiny interior residual)
Closing two §9 gaps: (a) cloud meadow+scattering validated on **more than cam 0**, (b) a **low-σ
interior** check (σ=7.5 clamps the dense interior to black on both sides, making that match partly
trivial). CUDA-MIS vs Mitsuba-analog, 512 spp × 3 seeds, box filter, g=0.85. Renders in `renders/p2/`.
- **View-independence (cam 6, cam 12 done):** bulk image matches Mitsuba to **~0.9%** (cam12 medians
  0.389 vs 0.386; p99s align). Unclipped energy Δ: cam6 **−0.0009 (0.7σ)**, cam12 **+0.0036 (2.8σ)**
  — small; the cam12 2.8σ is the same firefly-tail-noise effect as §8.8/8.11 (only 3 seeds), median
  flat. **Re-confirms firefly-free MIS off-axis:** CUDA max **0.7** vs Mitsuba-analog max **95.2**.
- **Metric caveat (important):** the `clipped(<2)` diff looks large (+0.07) **only as an artifact** —
  clipping strips Mitsuba-analog's real-but-spiky firefly energy (~0.07 of its mean lives in the
  tail), making the clipped comparison unfair to the noisy reference. The honest, energy-conserving
  metric is the **unclipped global / median**, which agree. Do NOT read clipped diffs as CUDA bias here.
- **cam 18 (done):** global **−0.0011 (0.8σ)** → PASS. View-independence now holds across cam
  **0/6/12/18** — the "cherry-picked view" objection is closed.
- **Low-σ interior (σ=2, const env, done):** global **+0.000179**, median|diff| 0.0035. Magnitude
  is tiny (~1.8e-4, <0.1% relative) but **21σ** — a *small but statistically real* systematic that
  the saturated σ=7.5 interior hid (gray interior, no fireflies → tiny SEM → small bias becomes
  visible). This is exactly what the low-σ test was for. **Not a blocker**; candidate causes: the
  σ-scaling/optical-depth normalization convention, or a small multiple-scatter difference. Left as
  an OPEN item (§9). Renders in `renders/p2/*lowsig*`.

### 8.14 Colored (per-channel RGB) albedo — tint correct; small channel-dependent residual
First test of a *tinted* medium (albedo R≠G≠B). Single Gaussian albedo=(0.9,0.5,0.2), σ=4, 2048 spp
× 3 seeds, CUDA vs Mitsuba-analog. Per-channel `SG_ALBEDO="r,g,b"` parsing added both sides
(`single_gaussian.cpp` `scatter_albedo3`, `render_single_gaussian_via_prb.py`). Renders in
`renders/rgb_albedo/`.
- **Tint is correct:** both renderers give R>G>B (CUDA 0.9894/0.9502/0.9238; Mitsuba
  0.9904/0.9479/0.9192) — the float3 albedo path works, no channel swap/collapse.
- **But a real channel-dependent systematic:** global +0.0020; per-channel R −0.0010, G +0.0023,
  **B +0.0046** — grows with absorption (B = lowest albedo = worst). Statistically huge (100–476σ)
  but small in magnitude (~0.1–0.5%). **Above** the ≤1e-4 we hold for grey albedo.
- **~~Likely cause: termination-threshold coupling~~ — RULED OUT (2026-06-05 investigation).** The
  §8.14 guess (Mitsuba cull `max(β)<0.005` + RR-off + `max_depth=32` vs CUDA `1e-4` + RR@5 + 128) was
  tested directly: rebuilt CUDA with Mitsuba's exact termination (`MAX_BOUNCES=32`, RR disabled,
  `MIN_THROUGHPUT=0.005`) and re-ran the 3-seed comparison. The residual was **byte-identical to 6
  digits** (R −0.000958 / G +0.002320 / B +0.004550 unchanged). Reason: a single Gaussian (σ=4) has
  short paths that escape the medium long before *any* termination threshold fires, so max_depth/RR/cull
  cannot be the cause. (Confirmed Mitsuba's scheme in `volprim_prb.py`: RR defaults OFF, cull
  `any(β>0.005)`, `max_depth=32`.)
- **Real cause is an estimator × colored-albedo interaction (narrowed, still OPEN).** Flipping CUDA to
  pure analog (NEE/MIS/analytic-direct all OFF) *changed* the residual dramatically — to **+0.057
  global, R-worst** (R +0.095 / G +0.052 / B +0.024), 28× larger and opposite channel order. So the
  validated default (MIS+NEE+RB on) matches Mitsuba-analog far better (+0.002) than CUDA's own pure-
  analog path does (+0.057). The residual lives in the colored-albedo handling of the analog/continuation
  path, not in termination. Next diagnostic: isolate whether the pure-analog +0.057 is a CUDA analog
  colored bug or a reference-estimator mismatch (what `mits_seed` actually integrates). Feature remains
  functional; the shipped MIS config residual is sub-percent (~0.1–0.5%).

### 8.15 Variance attribution — WHERE CUDA's noise actually is (redirects the A1 optimization)
Measured per-seed noise (std across seeds) CUDA vs Mitsuba-analog from existing seed sets, to test
the §8.5 "per-step RB" hypothesis BEFORE rewriting the core estimator. **Decisive — and it kills A1
as a priority:**
- **Env + MIS (the showcase, cloud meadow, 512spp×3):** CUDA per-seed noise **0.025** vs
  Mitsuba **0.165 unclipped / 0.024 clipped**. CUDA is **~6× cleaner on total noise** (firefly-free);
  on the bulk (clipped) the two are within ~5%. **MIS already closed the variance gap here.**
- **Constant env (low-σ cloud, σ=2, no env-IS benefit):** CUDA **0.0125** vs Mitsuba **0.0041** →
  CUDA **~3× noisier** — the §8.5 gap, now confirmed *clean* (no fireflies, clipped==unclipped).
- **Conclusion:** the 2.85×/3× variance disadvantage exists **only in flat/constant lighting**,
  where env-IS has nothing to importance-sample. In the env-map regime that the renderer actually
  showcases, **MIS already wins** (6× cleaner total, firefly-free; faster per-spp than Mitsuba-MIS,
  §8.11). So **per-step RB (A1) would help only the non-showcase flat-lit case (~3×) and add ~5% to
  the showcase** — a poor trade for a risky rewrite of the validated core estimator (`sampling.cuh`
  `sample_scattering_event` + `raygen.cuh`). NB both our argmin free-flight and Jorge's
  `volprim_prb` segment sampler are **analog**, so the §8.5 "Mitsuba folds transmittance every
  bounce" root-cause was a guess; the true flat-lit source (argmin overlap vs MIS-in-constant-env
  overhead vs shadow-ray variance) is being isolated on the `feature/a1-per-step-rb` branch.

### 8.16 Shadow-ray transmittance optimization — ~12–15× kernel speedup (branch feature/shadow-transmittance-opt)
Profiling the wavefront question (WAVEFRONT_PLAN.md) showed **~85% of frame time was the NEE/MIS
shadow-ray optical-depth integration** in `compute_transmittance_to_env`, running latency-bound at
30% occupancy. Root cause was algorithmic, not occupancy: it mirrored the primary-ray escape path —
build an event list, sort, march segment-by-segment summing `optical_depth` over all active prims per
segment = **O(events × active_prims) ≈ O(A²)** erf evals. But a shadow ray needs only the TOTAL
optical depth, and optical depth is **additive** across primitives, so each prim is integrated **once
over its full [entry, exit] span = O(A)**. Segmentation is only needed for scatter-DISTANCE sampling
on the primary ray.
- **Fix:** rewrote `compute_transmittance_to_env` to direct per-prim integration (no EventBuffer/sort).
- **Speed:** cloud cam0 scatter **98.7s → 6.5s (~15×)** const-env; meadow showcase **~83s → 6.6s
  (~12.5×)** (48 spp). Per-spp 2.06s → 0.135s.
- **Correctness:** identical to the pre-opt baseline — global mean diff **0**, mean abs diff **3e-8**,
  max **1.2e-3** (float summation order); **furnace 1.00011** (energy conserved). Since it matches the
  baseline that was validated ≤1e-4 vs Mitsuba (§8.4/8.8), it inherits that validation.
- **Implication:** this flips the performance story. Old §8.5: CUDA ~1.93× slower per-spp / ~5.5×
  equal-quality on flat env. With a ~15× kernel speedup CUDA is now **~7× faster per-spp than
  Mitsuba-analog**, i.e. **faster at equal quality essentially everywhere** (even the 2.85× flat-env
  noise penalty is now outweighed), and the env-map showcase win balloons. A clean equal-quality
  re-benchmark vs Mitsuba is the confirming TODO. NB this is the win the wavefront was chasing —
  obtained algorithmically, no kernel split needed.
- **Equivalence proof.** Total optical depth `τ = ∫ Σ_p σ_p = Σ_p ∫ σ_p` (linearity); per primitive
  the old per-segment sub-integrals telescope (shared erf endpoints cancel) to the full-span integral.
  Holds for every overlap config — geometry never enters. Empirically confirmed opt-vs-baseline:
  nested (small-in-big), partial/Venn, and disjoint overlap all reproduce to float-summation noise
  (mean Δ=0, mean abs ~1e-7; isolated ~1e-2 pixels are AA silhouette edges), plus cloud (max overlap
  ~40) at 3e-8 and furnace 1.00011.
- **Dead code removed** (commit after the opt): the rewrite orphaned the event list + the entire sort
  machinery. Deleted `EventBuffer` typedef, `device/core/sorting.cuh` (warp-shuffle/insertion/bitonic
  — the argmin primary path is sort-free, so the shadow path was its only caller), and the unused
  `include/thesis/device/optix/hit_event.h`. Build clean, render bit-identical (diff 0.0).
- **NEXT opportunity (unlocked by this).** The shadow integration is now an *order-independent* sum,
  so it no longer needs the stored hit list: the `anyhit` shader (`anyhit.cuh`) could **accumulate
  `optical_depth` into the ray payload during traversal** and never fill the 128-deep `HitBuffer` —
  which `constants.cuh` calls out as "the EXPENSIVE per-ray buffer" (LMEM blowup capping occupancy at
  ~30%). Eliminating it for shadow rays should lift occupancy → a further speedup on top of the 15×.
  Moderate change (new shadow-ray payload + anyhit accumulation), validated against the current result.

### 8.17 Post-optimization benchmark vs Mitsuba (cloud + meadow + HG, equal quality)
Re-ran the equal-quality benchmark after the §8.16 optimization (now merged to main). Methodology =
§8.5: equal-quality time ∝ k·t, k = (per-seed std)²·spp from the seed sets, t = measured s/spp.
cloud cam0, RTX 3090.

| renderer | s/spp | noise k | cost k·t | mean | correct? |
|---|---|---|---|---|---|
| **CUDA-MIS (optimized)** | **0.147** | 2.25 | **0.33** | 0.3215 | ✅ |
| Mitsuba-MIS (NEE on) | 2.66 | 3.04 | 8.08 | 0.820 | ❌ **+155% biased** |
| Mitsuba-analog | 0.667 | 4108 | 2740 | 0.3242 | ✅ (brute-force, firefly-heavy) |

- **Apples-to-apples (CUDA-MIS vs Mitsuba-MIS): CUDA is 24× faster AND correct**, while Mitsuba's own
  variance-reduced path converges to a **2.5× too-bright** image (the +6.5% furnace NEE bug §8.1,
  amplified to +155% in this volumetric+meadow scene). Mitsuba's *fast* path is *wrong* here.
- vs Mitsuba-analog (its only **unbiased** path): ~8260× (k·t; inflated by Mitsuba's firefly k=4108;
  fireflies converge slower than 1/√spp, so this *understates* the edge).
- **Per-spp throughput: CUDA is now 4.5× faster than Mitsuba-analog, 18× faster than Mitsuba-MIS** — a
  flip from ~1.93× *slower* per-spp pre-optimization (§8.5). The optimization moved CUDA from "slower
  per-spp, wins only via MIS variance reduction" to "**faster per-spp AND lower-variance AND correct**."
- CUDA-MIS k=2.25 is even below Mitsuba-MIS k=3.04 — CUDA is the cleaner estimator too.

### 8.18 Anyhit-transmittance fusion — small real win ~3% (branch feature/anyhit-transmittance-fusion)
Follow-up to the §8.16 "NEXT opportunity": fuse the shadow-ray `optical_depth` integration **into
traversal** so the 128-deep `HitBuffer` is never filled for shadow rays. A transmittance-mode `anyhit`
integrates each entered primitive over its [entry, exit] span and accumulates τ in a local-memory
scalar during the single GAS descent (`optixIgnoreIntersection` continues). One payload slot selects
COLLECT (primary/scatter, unchanged) vs TRANSMITTANCE mode — no host/SBT/pipeline changes.
- **Correctness: exact.** cloud cam0 128 spp seed0 vs `main`: global mean Δ **+2.8e-10**, mean|Δ|
  **2.6e-8**, max **3.4e-5**, RMSE **1.7e-7** (float summation order only; tighter than §8.16 itself).
  Furnace PASS (1.00004 thin / 1.00008 thick — energy conserved). Optical depth is additive, so
  order-independent inline accumulation = the buffered loop.
- **Speed: ~3% faster, small but real.** Thermally-controlled tight-interleaved A/B (fusion vs main,
  same `test_runner` binary, swapping only `device_program.optixir`, 64 spp, no cooldown so paired runs
  are ~30 s apart = matched thermal state). The four steady-state pairs (both builds at a settled ~27 s)
  were unambiguous: **−3.4 / −3.1 / −3.0 / −3.3 %**; all-12-pair median −3 %. Warmup/re-throttle pairs
  are noisy (±12 %) and discarded — the absolute times drift 27→36 s across the session, which is why
  only *paired* deltas are trustworthy.
- **Why only ~3% (and not the occupancy jump §8.16 speculated):** occupancy here is **register-limited**
  (linked pipeline ~114 regs), NOT local-memory-limited. The `HitBuffer` is LMEM, which does not gate
  register-limited occupancy — and it must stay declared for the primary-ray argmin regardless. So the
  fusion can't lift occupancy. The 3% comes from **removing the shadow path's LMEM writes
  (`collect_hits` emplace_back) + the re-read loop** — less local-memory traffic and fewer instructions
  in a latency-bound kernel. The §8.16 "lift occupancy by dropping the buffer" framing was wrong: the
  buffer was never the occupancy gate.
- **A dead end that wasn't:** moving the `HitBuffer` into the `__noinline__` `sample_scattering_event`
  frame (to keep it out of raygen's) **regressed ~15%** — OptiX penalizes a large local in a noinline
  continuation frame. Reverted. Left the buffer in raygen.
- **Verdict: KEEP.** Correctness-exact + ~3% faster + the shadow path no longer touches the 128-deep
  buffer (cleaner data flow), for the cost of one payload slot and a mode branch in the anyhit. Merge is
  user-gated. NB the real remaining lever is NOT here — see §9 / the register-limited note: the only way
  to drop the `HitBuffer` from the frame entirely is to also fuse the PRIMARY ray (argmin is a min, not
  a sum — wavefront-scale work, not a v1).

#### Adjacent levers tested alongside the fusion — both NEGATIVE (don't re-try)
- **`maxRegisterCount` sweep (occupancy lever) — NO EFFECT.** The main module already caps at 96
  (`module.h:33`); the linked pipeline still profiles ~114 regs. Round-robin sweep {64, 80, 96, 128,
  0=unlimited} × 3 cycles on cloud cam0 64 spp: per-cap means 30.05 / 30.11 / 30.28 / 32.45* / 30.04 s
  (*one thermal-spike outlier) — a <1% spread, far inside the ±15% thermal noise. **Register/occupancy
  tuning is not a lever here**: the kernel is latency-bound on **dependency chains (the erf arithmetic)**,
  not occupancy-starved, so adding warps doesn't help. Corollary: the productive levers REMOVE work
  (this fusion, the bounce-0 double-scan dedup) or REDUCE variance (low-discrepancy sampling), not add
  occupancy. module.h left at 96.
- **Shadow-ray τ-saturation early-out (`optixTerminateRay` when τ≥`MAX_OPTICAL_DEPTH`) — NEUTRAL on the
  cloud.** Implemented exactly (correctness-exact: mean Δ 2.3e-10 vs main), A/B vs plain fusion over 10
  tight pairs: mean +0.2%/−0.2%, scatter around zero. The cloud at σ-mult 7.5 rarely accumulates τ≥88
  along a shadow ray (env-facing rays exit before saturating), so the early-out almost never fires.
  Exact + zero-cost, but no gain on the validated workload → **left OUT** to keep the merged change
  minimal (per CLAUDE.md "no premature optimization"). It IS a correct one-line add for optically
  thicker media (denser clouds / higher σ) if a future scene needs it.

### 8.19 Bounce-0 origin-inside scan dedup — clean ~8% win (branch feature/dedup-bounce0-scan)
A "remove work" lever flagged by §8.18's register-cap result. At bounce 0 the analytic-direct term
(`raygen.cuh`) re-scanned ALL primitives with `point_inside_bvh_bound` to build the camera-origin-inside
set — the **exact same O(N) scan** `sample_scattering_event` had just run for the same origin. So every
primary ray did the full-scene point-inside test twice. Fix: `sample_scattering_event` hands its set back
via an out-param (captured before the argmin path modifies it); raygen reuses it.
- **Correctness: exact.** cloud cam0 128spp bit-identical to baseline (max|Δ| 3.4e-5, float-order only) —
  the captured set is the same scan the old code re-ran.
- **Speed: ~8% faster, rock-solid.** Tight-interleaved A/B (same binary, swap optixir; 64spp, 10 pairs):
  ALL 10 pairs negative; steady-state dedup ~23.10s vs predup ~25.11s = **−8.0%** (low variance, every
  pair −7.7…−8.9%). Bigger than the fusion's 3% — the removed scan is 652 point-inside tests × every
  primary ray × spp, and bounce 0 is the most-travelled bounce.
- **Takeaway:** confirms the §8.18 corollary hard — *removing work* is the lever on this latency-bound,
  erf-dependency-chain-limited kernel, not occupancy. Two work-removal wins now stack (fusion 3% +
  dedup 8%).

### 8.20 Owen-scrambled Sobol AA — measured, NO win, reverted (was branch feature/sobol-sampling)
Measure-first test of low-discrepancy sampling (the §8.15 prediction was that it would be marginal).
Implemented Owen-scrambled Sobol' (Burley 2020 hash-based scramble) for the **camera AA jitter** — the
ONE path dimension consumed with a fixed, deterministic index per (pixel, sample). The variable-count
argmin free-flight + post-it NEE directions can't be Sobol-stratified (data-dependent dimension count),
so they stayed on PCG. The PCG jitter draw was kept (and discarded) when Sobol was on, so the downstream
rng stream was IDENTICAL between the two builds — isolating purely the AA-stratification effect.
- **Equal-quality A/B** (meadow showcase, cloud cam0, vs a 1024spp reference, same seed so scatter noise
  is shared and cancels): RMSE reduction Sobol-vs-PCG = **−0.1% @16spp, −0.1% @32spp, +0.8% @64spp** —
  i.e. **zero within noise**. Region split @32spp: bright/env −0.2%, dark/cloud +0.2%. No win even on the
  high-frequency env background.
- **Why:** confirms §8.15 exactly. The image variance is dominated by the **scattering MC** (argmin
  free-flight + NEE), which is identical between the two builds; AA jitter is a negligible slice, so
  stratifying it moves nothing at these spp. Sobol can only help the dimensions QMC can stratify, and on
  this estimator that's just AA — which isn't where the noise is.
- **Verdict: REVERTED.** Per the measure-first rule ("keep only if it measurably beats PCG"), the code
  was removed (sobol.cuh + flag + raygen hook). The real variance lever would be reducing the
  scattering-MC noise, which QMC can't reach here — and MIS already makes the showcase variance-
  competitive (§8.11/§8.15). Don't re-attempt Sobol without first changing the estimator's
  sample-consumption structure to a fixed low dimension.

### 8.21 Fast transcendentals in the hot path — fast_erf KEPT (~1.5%), fast_acos REVERTED
Attacked the dominant arithmetic (`optical_depth` ≈ 85% of frame). **Context that shapes the result:
the Release/optixir build already uses `-use_fast_math --ftz=true --prec-div=false --prec-sqrt=false`
(cmake/OptiX-IR.cmake), so `erff`/`acosf`/`expf` are ALREADY their fast-math (leaner, lower-precision)
forms — this caps the headroom for hand-rolled approximations.**
- **fast_erf — KEPT but OPT-IN (cmake `THESIS_ENABLE_FAST_ERF`, default OFF), ~1.5%.** Default build
  uses exact `erff` so the validation path / Mitsuba comparison is unaffected; enabling the flag swaps
  in the approximation (gates `math::fast_erf`). `optical_depth` calls erf twice; we need only ~1e-4.
  Replaced with Abramowitz-Stegun 7.1.26 (rational × `__expf`): float32 max abs err ~5e-7, summation
  over ~40 prims ~3e-6 (≪ 1e-4 budget). A *pure polynomial* was rejected — degree-15 for 1e-6 in
  float64, and float32 Horner is unstable (1.3e-3) on the high powers. Even fast-math `erff` is ~46 SASS
  ops; A&S ~2× lighter. Validation: cloud-meadow self-diff meanD 3e-8 / max 3.4e-5; furnace PASS;
  single-Gaussian **meadow-vs-Mitsuba systematic UNCHANGED** (global +0.001543, median 0.0074 — no
  regression). Speed: tight A/B 10/10 pairs faster, steady-state ~1.5%. Small because the kernel is
  latency-bound (memory/traversal), not ALU-bound, and A&S still keeps one exp.
- **fast_acos — REVERTED (measured on both axes).** Eberly sqrt-based acos for the env-map polar angle
  (`sample()` ~530M calls/frame). Speed A/B (erf+acos vs erf-only): **~0%** (mean −0.3%, scatters
  −2.1…+1.9% — `acosf` is already cheap under fast-math, nothing to trade). Correctness: float32 angle
  err ~6.8e-5 rad (~0.04 texels) but the high-freq meadow turned it into a **+3.5e-4 systematic bias**
  (above the 1e-4 budget) via the sky→ground gradient. Zero speed + correctness cost → reverted.
- **Takeaway:** with fast-math already on, library transcendentals are hard to beat; only the heaviest
  with accuracy slack (erf) gave a small safe win. Env-map lookups are bias-sensitive on a high-freq
  HDR — approximate them only at near-exact accuracy, and there's no speed there anyway.

### 8.22 OptiX denoiser — evaluated, ~30× effective speedup (already implemented; branch feature/denoiser)
The denoiser was already fully built (host `optix/denoiser.h` + `--denoise` flag + albedo/normal AOV
guide layers captured at bounce 0 in raygen + `denoise_and_save`); this was an *evaluation* of it, no
new code. Run on cloud cam0 + meadow:
- 16 spp raw render = 5.2 s, visibly grainy. `--denoise` (OptiX HDR denoiser, albedo+normal guides)
  produces a smooth, clean image in the same launch.
- **Quality:** raw-16spp vs a 512spp reference RMSE = 0.384; **denoised-16spp vs 512spp RMSE = 0.070**
  → 5.5× lower error. Since raw noise ~1/√spp, matching the denoised error with raw samples would need
  ~486 spp → **~30× effective speedup** (16spp looks like ~486spp).
- **Caveat (thesis framing):** the denoiser is an *approximation* — it slightly over-smooths the fine
  wisp texture and is not the converged truth. Show it as a fast preview ALONGSIDE the converged render,
  not as a ground-truth result. The 0.070 RMSE includes denoiser bias, not just removed noise.
- **Status:** functional and a strong showcase differentiator (Mitsuba's reference path has no equivalent
  integrated denoise). Possible follow-ups: resolve the `denoiseAlpha` API TODO in denoiser.h; a denoised
  showcase bundle; temporal/multi-frame denoising is out of scope (single-frame renderer).

### 8.23 Skip per-bounce origin-inside scan — ~16%, the biggest win this session (branch feature/incremental-active-prims)
(NB §8.20–8.22 live on sibling branches feature/findings-sobol-rgb / fast-erf / denoiser; merge order
fills the gap.) `sample_scattering_event` scanned all 652 primitives with `point_inside_bvh_bound` at
EVERY bounce to find the origin-inside set (prims the ray STARTS inside — OptiX backface-culls these so
they're never reported as entry hits, hence the scan). Insight: after a scatter, `event.active_prims_`
already holds the scatter point's active set, and the next bounce starts AT that scatter point, so its
origin-inside set is IDENTICAL. Any prim the new origin is inside was crossed by the previous ray (it
passes through that point) ⇒ already captured. So scan only at bounce 0 (camera ray); bounce>0 inherits
the set for free via a `first_bounce` flag.
- **Speed: ~16%, the largest single win of the session.** Tight A/B (cloud cam0 64spp, 10 pairs):
  10/10 faster, steady-state −16.1% (−16…−19%). Stacks with the §8.19 bounce-0 dedup (orthogonal —
  that removed raygen's *duplicate* bounce-0 scan; this removes the *per-bounce* scans).
- **Correctness: UNBIASED but NOT bit-exact** (the only such opt this session). furnace PASS (1.00008);
  meanD vs baseline +3.9e-7 (≈0, no bias); meadow-vs-Mitsuba systematic UNCHANGED (global +0.001543).
  But max|Δ| = 0.177 at isolated pixels: at a grazing 3σ boundary the exit-distance test that builds
  `final_active_prims` and the containment test the scan uses can disagree by ε, flipping one prim and
  diverging that MC path. Zero-mean (no bias), and the 3σ cutoff is itself an arbitrary truncation
  (drops 0.3% of mass) so neither membership answer is "more correct"; the divergence averages out with
  spp. Merge decision is a judgment call vs the bit-exact fusion/dedup — but it's the biggest win and
  passes every correctness gate.
- **Cumulative throughput this session** (compounding, independent paths): fusion 3% × dedup 8% × erf
  1.5% × this 16% ≈ **~1.35× more throughput**, extrapolating §8.17 to **~6× per-spp vs Mitsuba-analog**
  and **~32× equal-quality vs Mitsuba-MIS** (still correct where Mitsuba-MIS is +155% biased). A fresh
  equal-quality benchmark would replace the extrapolation with a measured figure.

### 8.24 Showcase-quality options — firefly clamp + Gaussian filter (branch feature/showcase-quality)
Two opt-in beauty/robustness knobs, both DEFAULT-OFF so the validation path (and the Mitsuba
comparison) is bit-identical and unbiased. Motivated by reporting results in TWO modes — with and
without the denoiser — so the *non-denoised* showcase needs nicer AA / firefly safety than the raw
validation build.
- **Firefly clamp (`FIREFLY_CLAMP_LUMINANCE`, default 0).** Hue-preserving per-sample Rec.709-luminance
  clamp before accumulation. Compile-gated → 0 is a true no-op (bit-identical, meanD 2.8e-10). At
  threshold 2.0, max output luminance dropped to 1.36 (all per-sample spikes capped). BIASED (energy
  loss on clamped pixels) → beauty/robustness only. The MIS showcase is already firefly-free (§8.15),
  so this is insurance for other configs / pathological samples.
- **Gaussian reconstruction filter (`PIXEL_FILTER_STDDEV`, default 0 = box).** The earlier decision kept
  BOX because it's the validation-exact reconstruction (matches Mitsuba `rfilter=box`) — but that was a
  *validation* choice; for beauty the note always said "Gaussian/Mitchell if beauty needed." Implemented
  via **filter importance sampling (gather, not splat)**: when stddev>0 the camera sub-pixel jitter is
  drawn from the Gaussian kernel (Box-Muller, support ±2px) and accumulated weight-1, so adjacent pixels
  gather overlapping regions = softer edges — **no atomics, no accumulation restructure**, only the
  jitter distribution changes. (My initial "needs splatting" claim was wrong; FIS-gather avoids it.)
  Validation: box default bit-identical (meanD 2.8e-10); stddev=0.5 (Mitsuba-like) gives meanD +1.4e-4
  (≈0, energy preserved), meanAbs 6.9e-2 edge-localized softening, furnace PASS (1.00008, flat field
  stays flat). Box stays the Mitsuba-comparison default; Gaussian is for the beauty / non-denoised
  showcase mode.
- **Note on the denoiser overlap:** the denoiser (§8.22) subsumes most reconstruction-filter smoothing,
  so the Gaussian filter matters mainly for the *non-denoised* beauty mode (and for figures where the
  denoiser's approximation is unwanted).

### 8.25 Generalization benchmark vs Jorge's volprim_prb on DSYG paper assets (WDAS Disney cloud + embergen)
First head-to-head on the DSYG paper's OWN benchmark assets (not our 652-G toy cloud). Of the 24
downloaded zips (full taxonomy: `ASSET_TAXONOMY.md`), only the `_gauss` variants are pure-Gaussian
fits we can render (16k–25k Gaussians); the rest are Gabor fits (Gabor is NOT in DSYG — separate
follow-up). Rendered two: **wdas8_gauss** (WDAS Disney cloud ⅛-res, 24,576 G — the paper's Fig.1 hero)
and **embergen_gauss** (combustion plume, 24,576 G).

**Pipeline.** CUDA side: `tools/refs/npy_asset_to_ply.py` converts the asset's `npy_data` → our PLY
(scale=log, quat xyzw→wxyz, sigma_t=opacity). Mitsuba side (new `tools/refs/render_asset_via_prb.py`):
loads the NATIVE PLY as volprim `ellipsoids` with a custom perspective sensor matching our
`asset_validation` camera; the native PLY's density property `opacities_0` is renamed→`sigma_t_0`
(header-only; this volprim build's `volprim_prb` requires a `sigma_t` attribute). Both renderers thus
render the SAME Gaussians, same camera.
**Config (matched):** 512², 64 spp, seed 0, constant white env, albedo 0.9, HG g=0.85, max_depth 128,
density scale wdas=10 / embergen=20 (per `reference_asset_density_scales`). RTX 3090. CUDA built at
`MAX_ACTIVE_PRIMS=HIT_BUFFER_CAPACITY=512` (see cap finding below). Two Mitsuba variants per the
request: ANALOG (`use_nee=0`, the trustworthy reference) and NEE (`use_nee=1`).

**Results (per-spp, 64 spp):**

| asset | CUDA | Mits-ANALOG | Mits-NEE | CUDA/analog energy | RMSE CUDA vs analog |
|-------|------|-------------|----------|--------------------|---------------------|
| wdas8_gauss (Disney) | **59.0 s** | 57.2 s | 193.5 s | **0.9999** (✓) | 0.028 (noise-limited) |
| embergen_gauss | 59.5 s* | 93.6 s | 551.3 s | n/a (invalid*) | n/a* |

\* embergen CUDA render is INVALID for quality: at caps=512 it STILL drops 63.5M cap entries
(under-absorption bias) AND has 0.34% NaN pixels (degenerate grazing Gaussians). Perf only.

**Findings:**
1. **Correctness — we generalize to the paper's hero asset.** On the WDAS Disney cloud, CUDA matches
   Mitsuba-ANALOG energy to **0.01%** (ratio 0.9999); residual RMSE 0.028 is MC noise at 64 spp (both
   estimators noisy). The renderer is correct beyond the toy cloud.
2. **Mitsuba-NEE is energy-biased +1.2% brighter** than analog on wdas8 (same class as the +6.5%
   furnace bias on the toy cloud) — confirms analog as the reference, and that our analog+RB estimator
   is unbiased where Mitsuba-NEE is not.
3. **Per-ray cap scaling is the real limit.** The 128-caps tuned for the 652-G toy cloud (max overlap
   ~45) overflow CATASTROPHICALLY on dense real assets: at 128 caps wdas8 drops 300M entries, embergen
   741M → heavily biased (too bright). Overflow vs caps: wdas8 {128:300M, 256:2.6M, 512:0}; embergen
   {128:741M, 512:16M, still>0}. So WDAS needs caps≥512; embergen needs >512. **Raising caps is ~free
   here** (~14 s/16spp at 128/256/512) — these assets are traversal-bound, NOT the ~6× buffer penalty
   the old sparse-cloud note predicted. The proper fix remains graceful overflow (#63), not bigger caps.
4. **Perf regime shifts with density.** On the dense WDAS cloud CUDA only TIES Mitsuba-analog per-spp
   (59.0 vs 57.2 s; ~2× faster on the toy cloud) — deep overlap (≤512 prims/point) erases the BVH
   traversal edge; both do the same heavy erf integration over hundreds of overlapping Gaussians. But
   CUDA is **3.3× faster than Mitsuba-NEE** (wdas8) and **9.3×** (embergen) — NEE is the variant you'd
   use for clean images. Equal-quality (not per-spp) should favor CUDA MORE, since our NEE+MIS+analytic-
   direct is lower-variance than analog at equal spp — NOT yet quantified (needs a multi-seed pass).
5. **Bugs surfaced:** the `asset_validation` perspective camera is VERTICALLY FLIPPED (CUDA-flipped
   aligns with Mitsuba at RMSE 0.028 vs 0.199 direct) — confirms the known Phase-2 flip bug. embergen
   shows 0.34% NaN (degenerate Gaussians; Phase-2 NaN class — fixed in §8.26).

**Caveats / not-yet-done:** constant white env only (no env-map orientation matching for assets);
equal-quality (variance-matched) speedup not measured; embergen needs caps>512 + the NaN fix for a
valid quality comparison; perf is single-camera (negz), not the asset's 32-cam rig.

### 8.26 Dense-asset NaN — root cause + fix (branch fix/asset-nan)
The dense DSYG assets (WDAS cloud, embergen — 24k Gaussians, deep overlap)
produced NaN pixels (embergen 8 px at caps=128, 2658 at caps=512; sampling-dependent — 0 at 1 spp,
appears at 64 spp; grows with overlap depth). Instrumented per-radiance-term probes pinned it: the
scatter **position was ±inf**, so `base = throughput·albedo(inf_pos)` was NaN and every downstream
NEE term inherited it.
- **Root cause:** the hit-buffer free-flight loop in `sample_scattering_event` (`sampling.cuh`)
  checked only `t_scatter < t_scatter_min`, **missing the `t_scatter >= 0` guard the active-prims
  loop directly above it already has**. A degenerate primitive makes `inv_cdf_segment` saturate
  `erfinv(±1) → -inf`; that `-inf` passed `-inf < t_scatter_min` and then `-inf <= t_exit`, so
  `t_scatter_min = -inf` → `position = ray.at(-inf) = ±inf` → NaN albedo/radiance.
- **Fix (2 layers):** (1) ROOT — add `t_scatter >= 0.0f` to the hit-buffer loop (mirrors the
  active-prims loop; rejects negative, -inf, NaN; +inf already rejected by `< t_scatter_min`). An
  invalid free-flight distance is correctly "no scatter" — unbiased. (2) DEFENSE-IN-DEPTH — reject
  non-finite per-sample `radiance` before the Welford accumulate (the existing check guarded
  throughput, not radiance). Standard PBRT/Mitsuba non-finite rejection.
- **Verified:** embergen 64 spp NaN **8 → 0** with the safety net firing **0 times** (the root fix
  handles it alone, not masked); validation cloud **BIT-IDENTICAL** (max|Δ|=0 — the guard only
  rejects degenerates that never occur on validated scenes); bunny NaN-free across configs (its
  documented σ=7.5 NaN did not reproduce at meadow/0.9 even pre-fix, so no A/B there). NOT NaN-bias:
  the guard removes spurious events, it does not drop real ones.

### 8.27 Flat-env variance gap = collision vs track-length estimator (A1 re-opened & corrected; branch feature/analog-indirect-diagnostic)
Re-investigated WHY CUDA is noisier than Mitsuba-analog on flat/constant-env high-albedo media (the
old "A1" question; §8.5 measured ~2.85× per-sample, §8.13/§8.15). Added two diagnostic compile flags
(`ANALOG_ESCAPE_ONLY`, `ANALOG_ABSORPTION`, default OFF — on branch) + a max-depth sweep. **Both my
initial hypotheses were measured WRONG**, which is the point of measuring:
- Per-seed noise, single-G const-env (σ=2, albedo 0.9, 6 seeds), by max depth:

  | depth | CUDA-NEE | CUDA weighted-analog | CUDA analog-absorb | Mitsuba-analog |
  |------:|---------:|---------------------:|-------------------:|---------------:|
  |   1   | 0.00599  | 0.00002              | 0.00002            | 0.00377        |
  |   2   | 0.00639  | 0.00581              | 0.00615            | 0.00096        |
  |   4   | 0.00643  | 0.00592              | 0.00630            | 0.00041        |
  |   8   | 0.00643  | 0.00592              | 0.00630            | 0.00041        |

  All CUDA variants are FLAT with depth (~0.006); only **Mitsuba-analog DROPS** (0.0038→0.0004).
  Means matched across all (unbiased — my analog paths are correct, the hypotheses were just wrong):
  neither the continuation-escape nor true analog absorption recovers the self-averaging.
- **Root cause (from reading Jorge's volprim_prb, NOT guessing):** Mitsuba folds the **analytic**
  segment transmittance into throughput every segment — `β *= seg_tr` (`volprim_prb.py:554`) — a
  **track-length / expected-value estimator** (every path contributes a smoothly exp(−τ)-weighted
  escape). CUDA's ADT/argmin design is a **collision estimator** (binary scatter-vs-escape coin; the
  transmittance is consumed by the decision, never folded into β). Track-length beats collision for
  transmittance-weighted estimands (textbook) → Mitsuba's variance vanishes with depth in the
  conservative limit; CUDA's stays flat.
- **This OVERTURNS the prior A1 conclusion** (A1_INVESTIGATION.md: "both estimators are analog, no
  difference") — that reading mistook `β *= seg_tr` for a mere free-flight accumulator. The ORIGINAL
  §8.5 premise (Mitsuba folds analytic transmittance every bounce; CUDA only at bounce 0 via
  ENABLE_ANALYTIC_DIRECT) was RIGHT. Confirms via the table: CUDA depth-1 ≈ 0 noise *because*
  bounce-0 analytic-direct already folds it; the gap opens once the binary continuation takes over.
- **Verdict: real lever, NOT implemented.** Recovering it means giving CUDA track-length throughput —
  a core-estimator rewrite that undoes the sort-free ADT/argmin novelty — and it only helps the
  flat/constant-env regime; the actual showcase (cloud + meadow + MIS) already beats Mitsuba (§8.11,
  §8.25). Documented as a characterized trade-off; a hybrid (keep argmin traversal but fold analytic
  transmittance into β, generalizing the bounce-0 analytic-direct to all bounces) is the open
  thesis-worthy direction if the flat-env gap is ever worth closing. Diagnostic flags + depth-sweep
  scripts live on branch feature/analog-indirect-diagnostic for reproduction.

### 8.28 Wavefront re-examined via ncu — occupancy IS a lever (corrects earlier null); but wavefront is double-edged
Clean Nsight Compute profile of the render kernel (`optixLaunch`, 114 regs/thread, block 128) to settle
the long-standing wavefront question. **`ncu` invocation** (RTX 3090, render kernel only):
`ncu --kernel-name "regex:optixLaunch" --launch-count 1 --section Occupancy/SchedulerStats/WarpStateStats/SpeedOfLight`
on a representative scattering render (`asset_validation`, bunny PLY, meadow, albedo 0.9, σ=7.5, 16 spp).
Resolution matters for the grid-fill: profile at **SG_RES=256** (512 blocks ≈ 6/SM — proper fill);
128² is too small (tail artifact → 8% occ) and 512² made ncu error out (NaN/exit-9, multi-second kernel).

**Numbers @256²:**
- Compute (SM) 34.5%, **DRAM 1.1%** → pure **latency-bound** (neither ALU nor bandwidth saturated).
- **Achieved occupancy 21.7%** (10.4 warps/SM) vs ~37% register-limited ceiling (114 regs).
- **Eligible warps/scheduler 0.44; No-eligible 66%**; ~8.2 warp-cycles per issued instruction → scheduler starved.
- **Stall breakdown** (avg warps stalled/reason): **long_scoreboard 4.10 (memory latency, DOMINANT)**,
  **wait 1.99 (arith dependency)**, not_selected 0.28, no_instruction 0.15, lg_throttle 0.12,
  mio 0.04, short_scoreboard 0.03, **math_pipe_throttle 0.03** (≈0), barrier 0.
- Local-memory traffic: 882M local-ld + 548M local-st instr, 2.56B local-ld sectors (per-ray
  HitBuffer/CompactSet + 114-reg spills), on top of global primitive-data loads.

**Verdict — this REVERSES the prior "occupancy is NOT the lever" note.** That null came from a
*confounded* maxregcount sweep (capping regs raised occupancy by SPILLING, which added the very memory
latency it tried to hide → net null). The clean profile shows occupancy genuinely low (22%) and the
scheduler genuinely starved (0.44 eligible, 66% no-eligible) → **more warps WOULD hide the latency.**
The kernel is latency-bound, dominated by MEMORY latency (long_scoreboard), then arithmetic-dependency
(wait); zero throughput-throttle. So occupancy is a real lever.

**But wavefront is double-edged, NOT a slam-dunk:**
- PRO: split megakernel → fewer regs/stage → genuinely higher occupancy (no spill) → hides latency.
- CON: wavefront moves per-ray state (ray o/d, throughput, radiance, RNG, ScatteringEvent, ~100–200 B)
  to **GLOBAL memory**, streamed read+write EVERY bounce — adding high-volume traffic on the EXACT axis
  (memory latency) that is already our #1 stall. The megakernel keeps that state on-chip (zero global
  round-trips). So wavefront trades register-pressure-limited-occupancy for global memory traffic, and
  we're already memory-latency-bound. Plus: per-bounce stream-compaction overhead (most paths die by
  RR depth 5) and N launches/frame.
  NB (corrected): the in-raygen bounce loop is merely IDIOMATIC (the OptiX SDK optixPathTracer sample's
  shape), NOT "the only natural OptiX shape" — OptiX 7+ explicitly supports wavefront usage (trace-only
  raygen, path state in global, shade/compact in separate CUDA kernels, optixLaunch per bounce; many
  production throughput renderers do this). So "fights OptiX's grain" is NOT a valid con — retracted.
  Conversely, OptiX Shader Execution Reordering (the built-in megakernel-divergence fix) is Ada-only;
  on this Ampere 3090 there's no SER, which if anything strengthens the wavefront load-balance case.

**RECOMMENDATION (do this FIRST, before the 5–7 day wavefront rewrite):** reduce the **per-ray state
footprint** — the 128-deep HitBuffer + CompactSet + 114-reg spills. It hits BOTH confirmed levers at
once — raises occupancy AND cuts the dominant long_scoreboard stall — with NO added global traffic
(strictly better on both axes; wavefront wins one, risks the other). It is also wavefront's own
prescribed first step ("primary-ray HitBuffer fusion" in WAVEFRONT_PLAN.md) and pairs with
graceful-overflow (#63) to simultaneously fix the dense-asset cap-overflow (§8.25). Only if that
plateaus AND occupancy is still the wall is the full wavefront rewrite justified BY DATA (it wasn't
before). Profiles saved: /tmp/ncu_prof256.txt (+ the stall-metric query in this session's transcript).

### 8.29 Megakernel footprint-reduction tested — it is NOT the lever; bottleneck is global Primitive-load latency (corrects §8.28's recommendation)
Acting on §8.28's "reduce per-ray-state footprint FIRST," I implemented the two cleanest footprint
cuts and measured each (ncu @ bunny `asset_validation` SG_RES=256, 16 spp; wall-clock on the cloud
cam0 σ=7.5 albedo=0.9 256 spp). **Both are bit-identical** (0/1.62M px differ vs baseline) and kept on
main as hygiene, but **neither moved wall-clock** — and the profile shows why §8.28's premise was wrong.

**Change 1 — SoA HitBuffer (drop the dead `is_exit`).** The argmin path stores only entry hits, never
sorts, never reads `is_exit`; the AoS `HitRecord` (float+uint16+`uint16 is_exit:1`, padded to 8 B)
wasted a 2-byte word on a flag always-false-never-read. Split into parallel `float t_hit_[]` +
`prim_idx_t prim_idx_[]` → 8 B → 6 B/entry (25% off the dominant per-ray *local* buffer).
**Result: BIT-IDENTICAL, zero speedup (70 s → 69 s, noise).**

**Why it can't help — the smoking gun.** ncu of the megakernel: **global loads 12.5 BILLION instr vs
local ld+st 0.15 B — local is ~1.2% of memory traffic.** The dominant stall (long_scoreboard) is driven
by **scattered *global* loads of the `Primitive` structs** (read repeatedly in the
`inv_cdf`/`inv_cdf_segment`/`optical_depth`/`pdf` loops), NOT the local HitBuffer/CompactSet that §8.28
assumed. Shrinking local state was always bounded by ~1% — confirmed. **§8.28's "footprint is the lever"
recommendation is empirically refuted as a perf lever** (it remains valid as cleanup / future-RayState prep).

**Change 2 — `Primitive` hot/cold field reorder.** `sizeof(Primitive)=80 B` (alignas 16). Every device
hot-path method reads exactly 64 B (center, rot_quat, rcp_scale, density_norm_factor, inv_cdf_factor,
albedo, optical_thickness) but **never `scale_`** (transforms use `rcp_scale_`; `scale_` is host-only,
BVH `localToWorld`/export). It sat at offset 28 — mid-hot-fields — so every device load dragged its
cache sectors. Moved `scale_` to the **last** data member (offset 64, verified; `static_assert offsetof
≥ 64` pins it) so the hot 64 B occupy the first two 32 B L2 sectors and the device never fetches the third.
**Result: BIT-IDENTICAL.** ncu before→after (same config): **L1 sector hit 81.6% → 83.8% (+2.2 pt),
long_scoreboard 3.65 → 3.43 (−6%)**, global_ld instr unchanged (12.518B→12.509B; same # loads, fewer
sectors/load), occupancy 23.4% → 22.7% (noise). **Mechanically correct (better L1 residency, fewer memory
stalls) but small — below cloud wall-clock noise (~70 s → ~69 s).**

**Consolidated verdict (3 experiments incl. the §8.18 anyhit fusion — all point the same way):** the
megakernel is **occupancy-limited (114 regs → ~22%) and global-Primitive-load-latency bound.** Per-ray
*footprint/layout* micro-opts shave the latency a few % but **cannot overcome 22% occupancy** — at this
occupancy the SM can't hide the remaining global-load latency no matter how tidy each warp is. The L1 hit
is already 82–84% and L2 98.7%, so the data largely *resides* in cache; the wall is **latency exposure
under low occupancy**, hideable only by **more warps**. Raising occupancy needs a genuinely smaller live
register set, which the megakernel can't do (the 2026-06-05 maxregcount cap just spills → wash, §8.28).
The only structural path to more warps is the **wavefront split** (separate high-occupancy integration
kernel) — with its known tradeoff (adds global RayState traffic on this same dominant axis). **So the
wavefront premise stands as the only big lever, but its core risk — global-memory traffic — is now
confirmed to be exactly where we are already bound, which is precisely why §8.28 called it double-edged
and why a Stage-2a prototype must MEASURE the net before committing to the full rewrite.** A remaining
non-rewrite option not yet tested: cut the global-load *instruction count* (the `Primitive` is re-read
~4–5× per scatter — argmin / rebuild-exit / evaluate_albedo / NEE×2); caching exits or fusing reads could
drop the 12.5B, but it touches the validated estimator (bias risk) and is still bounded by the 22%
occupancy ceiling. (Profiles this session: /tmp/mk_exp/ncu_soa.txt, ncu_stalls.txt, ncu_reorder.txt.)

**Change 3 (tested, REVERTED) — route Primitive loads through the read-only cache (`__ldg`).** Loaded
the hot 64 B per primitive via `__ldg` (LDG.CI) in the six hot loops (argmin active/hit, rebuild
active/hit, evaluate_albedo, transmittance). **Bit-identical** (same bytes). ncu: long_scoreboard
3.43 → 3.29 (−4%, cumulative −10% from the 3.65 baseline across all three changes), but **L1 sector hit
ticked DOWN 83.8% → 83.3%** and **wall-clock flat (~71 s vs ~70 s baseline)**. Expected: the data is
already L1/L2-resident (82–84% / 98.7%), so the read-only path has nothing to add. **Reverted** — it adds
a non-obvious 80B-reinterpret `load_prim` helper (leaves `scale_` as garbage by design) for zero
measurable wall-clock and a slightly worse L1 hit. NOT worth the fragility. Confirms the thesis a third
way: per-load latency micro-opts can't convert to throughput while occupancy (22%) is the wall.

**Host-side + SER audit (no code change, for completeness).** Pipeline prep is already near-optimal:
`numPayloadValues=4` (min; AnyHit needs a 64-bit ptr+mode), `numAttributeValues=0`, module
`maxRegisterCount=96` (already capped; sweep-confirmed tapped out), `optLevel=LEVEL_3`/`debug=NONE`,
`maxTraceDepth=1`. Only untapped host lever is pipeline **bound-value specialization** (bake launch
constants → save a few regs/branches), but that would undo the Phase-1 runtime flags for negligible
gain. **OptiX SER** (`optixReorder`, the hardware fix for our exact divergence — 20.5/32 active
threads/warp) is available in the SDK (OptiX 9.0) but **requires Ada (SM 8.9+); the 3090 is Ampere
(SM 8.6) → SER is a no-op here.** Relevant only if final benchmarks move to a 40-series card, where SER
could lift occupancy WITHOUT a wavefront rewrite (wavefront's compaction is the software substitute for
the SER hardware we lack on Ampere). **Primitive-array SoA was also ruled out** (not implemented): our
access reads ALL fields of a DIFFERENT scattered prim per thread → AoS is cache-optimal; SoA would
scatter each prim across N arrays → strictly worse. The hot/cold reorder (Change 2) is the right AoS move.

**Better-ROI direction than wavefront (algorithmic, no occupancy fight):** the §8.5 gap is
~1.93× per-spp × ~2.85× per-sample variance. Megakernel/wavefront only attack the 1.93×. The larger
2.85× variance is attacked by **finishing adaptive sampling (#56, scaffolded)** — stop sampling converged
pixels (the cloud has large fast-converging regions) — and a **lower-variance estimator** (track-length,
§8.27). These close equal-quality time with zero occupancy fight, and are cheaper/lower-risk than the
5–7d wavefront rewrite. Work-removal in the erf loop (skip negligible-density prims) is the one lever
that also cuts the dominant global loads, but it edits the validated estimator.

### 8.30 Adaptive sampling implemented + evaluated — NET LOSS on the scattering showcase (kept, default-off)
Finished the scaffolded adaptive sampler (#56) as a proper runtime feature and measured it against
Mitsuba-style references on the cloud. **Verdict: it does not help our high-variance volumetric
showcase — slightly-to-2× SLOWER at equal quality — and introduces a small bias that fails the strict
systematic gate. Kept in the codebase (runtime-gated, default OFF = zero cost) as a measured result.**

**Implementation.** Promoted from compile-time (`ENABLE_ADAPTIVE_SAMPLING`/`ADAPTIVE_THRESHOLD`) to
runtime via RenderParams: `--adaptive-threshold` (0 = off → variance buffer not allocated, no per-sample
M2, identical to a non-adaptive render) and `--adaptive-min-samples`. The device keys all adaptive work
on `image_.variance_ != nullptr`. **Fixed a latent bug in the scaffolding**: the convergence criterion
was coefficient-of-variation `std/mean` (a per-distribution constant — never tightens with n, not an
error bound), now the **relative standard error of the mean** `sqrt(M2/((n-1)·n))/mean` (PBRT/Mitsuba
style), so the threshold means "stop at X% estimated relative error." Per-batch test (BATCH_SIZE=16,
default min 32). Output reads the Welford mean directly, so varying per-pixel counts are
output-correct. Plumbed through app + test-runner config paths.

**Measurements** (cloud_asset_scattering cam0, σ=7.5, albedo 0.9, RTX 3090; GT = uniform 2048 spp seed99,
530s; RMSE vs GT):
| config | time | RMSE | bias Δmean |
|---|---|---|---|
| uniform 256 | 76 s | 0.0236 | +0.00001 |
| adaptive max256 thr 0.01 / 0.02 / 0.04 | 75–76 s | 0.0238 / 0.0240 / 0.0249 | −0.0003 … −0.0005 |
| **adaptive max2048 thr 0.02 / 0.01** | **582 / 599 s** | 0.0128 / 0.0118 | −0.0004 / −0.0003 |
| uniform ~1024 (RMSE-interp) | ~304 s | ~0.0118 | — |

So adaptive max2048 thr0.01 matches uniform-1024 quality (RMSE 0.0118) but takes **599 s vs ~304 s — ~2×
slower**. At max256 it's a wash on time and slightly worse on quality. **Every operating point is
slower-or-equal, never faster.**

**Root cause — TWO compounding factors (high variance × SIMT divergence):**

*(1) Too few pixels converge.* The cloud is uniformly high-variance: firefly-prone scatter gives per-pixel
CoV ≈ 1–2, so reaching 2% relative error needs ~(CoV/0.02)² ≈ 2,500–10,000 spp — beyond any practical cap.
At thr0.02/max2048, essentially zero pixels stop early, so the per-batch M2+check overhead (~10%) is pure
loss → 582 s > 530 s. Adaptive's premise (many easy pixels to skip) doesn't hold for a frame-filling cloud.

*(2) SIMT warp divergence makes even the pixels that DO stop save nothing.* The convergence check does a
per-thread early `return` before the batch loop. But threads execute in lock-step warps of 32: a warp runs
until ALL its lanes finish, so a converged lane that returns just goes **idle** while its 31 warpmates keep
tracing the batch — it does NOT shorten the warp's runtime. Wall-clock is saved ONLY when an **entire warp**
(all 32 spatially-adjacent pixels) converges together and the warp slot frees. The force-stop diagnostic
(threshold 1.0 → every pixel stops at the 32-spp min → every warp converges coherently) rendered in **10 s
vs 76 s (≈32/256)**, confirming savings DO materialize under *coherent* convergence. But realistic
convergence is **scattered**: an easy pixel sits next to a hard cloud pixel in the same warp, so warps
rarely converge as a unit. The signature is in the max256/thr0.04 run — the image **mean shifted**
(−5e-4, so pixels demonstrably stopped early) while **wall-clock stayed 75 s** (warps kept running for the
non-converged lanes). So per-pixel adaptive in a megakernel is gated by the SAME warp-divergence wall that
limits occupancy (§8.28: 20.5/32 active threads per warp) — and it actually *worsens* divergence by idling
lanes. (Earlier note "rules out SIMT divergence" was wrong: the force-stop only shows the *coherent* best
case; divergence is precisely why *scattered* partial-warp convergence yields zero wall-clock benefit.)

**Bias caveat.** Early stopping on the right-skewed (firefly) scatter distribution locks in slightly-low
estimates before rare bright paths arrive → **negative bias ~−4e-4 (~6e-4 relative), which EXCEEDS the
≤1e-4 Mitsuba systematic gate.** So adaptive is beauty-only, never for the validation comparison.

**Where it would pay off (not here):** (a) genuinely low-variance scenes (absorption-only, simple
lighting), or (b) a **wavefront architecture with stream compaction** that removes converged rays from
the work pool regardless of spatial coherence — i.e. the same wavefront direction §8.28/§8.29 point to.
Adaptive is kept default-off (zero overhead) so it composes for free if wavefront ever lands. Scripts:
tools/refs/exr_rmse.py + the threshold sweep in this session's transcript.


> **Update (§8.34):** the wavefront escape-hatch this section hoped for was implemented and
> measured a dead end — so adaptive sampling has no remaining path to a win on this renderer and
> stays default-off as a documented negative result.

### 8.31 Density-contribution culling tested — DEAD END (redundant with the BVH's 3σ bound); reverted
Hypothesis (from the §8.29 bottleneck analysis): skip the exp + two erf() in optical_depth / inv_cdf /
inv_cdf_segment for primitives the ray only grazes in their far tail (whitened closest-approach perp²
large ⇒ exp(-0.5·perp²) negligible) — a true *work-removal* that should help at any occupancy, unlike the
latency micro-ops (§8.29). Added a runtime-tunable cutoff DENSITY_CULL_PERP2 (early-out after the cheap
perp² is computed) and swept it on the cloud (cam0, σ=7.5, albedo0.9, 256 spp; bias vs same-seed no-cull;
quality vs uniform-2048 GT):

| cutoff (≈σ, e_term) | time | vs no-cull | RMSE vs GT |
|---|---|---|---|
| OFF | 71 s | — | 0.0236 |
| perp²≤18.4 (1e-4) | 74 s | **bit-identical (0 px)** | 0.0236 |
| perp²≤13.8 (1e-3) | 75 s | **bit-identical (0 px)** | 0.0236 |
| perp²≤9.2 (3σ, 1e-2) | 76 s | **bit-identical (0 px)** | 0.0236 |
| perp²≤6.0 (2.45σ, 5%) | 76 s | 1.1M/1.6M px, **+0.9% mean** | 0.0261 (worse) |

**Verdict: not a good addition — reverted.** Culling out to **3σ is bit-identical (zero pixels change)**,
which proves the renderer **never evaluates a primitive whose closest approach exceeds 3σ**: the OptiX BVH
already wraps each Gaussian's 3σ surface and the active-prims / hit-buffer sets only ever hold near
primitives. So there is **no far-tail work to remove — the cull is redundant with the BVH's spatial
culling** (and even adds a sliver of overhead: 71→74-76 s). The first cutoff that culls *anything*
(perp²≤6 ≈ 2.45σ, where a Gaussian still carries ~5% density) immediately changes 68% of pixels with a
**+0.9% brightening bias** (≫ the ≤1e-4 systematic gate) — because in a dense overlapping cloud those
"tail" densities SUM into real optical depth (that overlap is literally what makes it dense). There is no
operating window between "removes nothing" and "biases heavily."

Broader takeaway (consistent with §8.29): the renderer's erf work is all on *genuinely-contributing*
primitives — there is **no redundant per-primitive compute to trim** in the megakernel. The remaining
real levers are unchanged: occupancy (wavefront) for throughput, and variance reduction (track-length
§8.27, path guiding, adjoint RR) for the equal-quality gap — NOT work-removal micro-ops. Cull code
reverted; only this note + the exr_diff/exr_rmse comparison tools are kept.

## 9. Known limitations & OPEN items

- **Feature validation (this session, §8.6–8.13) — summary.** Real HDR env (meadow), HG
  anisotropy, AND MIS are now all VALIDATED vs Mitsuba; the combined money shot matches and
  *beats* Mitsuba on the env scene (§8.11: ~630× vs analog / 1.5× vs its biased NEE, firefly-free);
  path-control knobs (RR / bounces / min-throughput) proven **unbiased** (§8.12); view-independence
  + low-σ coverage underway (§8.13). **Three real CUDA bugs found and fixed**,
  each latent because earlier tests couldn't see it: (1) env map rendered **upside-down**
  (`hdr.cpp flip_vertical`, §8.6) — hidden by constant env / equatorial camera / isotropic
  single-scatter; (2) **HG anisotropy sign** flipped vs Mitsuba (§8.9, `phase::sample` g_eff=−HG_G)
  — HG never exercised before; (3) **`phase::eval` sign** inconsistent with `phase::sample` (§8.10)
  — `eval` only used by MIS/env-IS, gave −52% energy until fixed to +HG_G. MIS now matches the
  validated NEE estimator to 0.7σ with ~159× variance reduction. Current validated default build:
  `HG_G=0.85` (forward), `ENABLE_MIS=true`, `ENABLE_NEE=true`, `ENABLE_ANALYTIC_DIRECT=true`.
  NB the committed value of `HG_G` (0 vs 0.85) is a user choice — both are validated.
- **Scattering at albedo>0 is VALIDATED end-to-end** (§8: furnace → single → clusters →
  full cloud, all matching Mitsuba analog to ≤~10⁻⁴). Open: (a) the §8.3 traits overlap
  residual (+0.0002, below detection at cloud scale); (b) CUDA is ~5.5× slower at equal
  quality **on the constant-env absorption cloud** (§8.5) — but ~630×/1.5× *faster* on the
  env-map MIS scene (§8.11); the per-spp gap is fixable via per-step Rao-Blackwellization +
  profiling; (c) cloud scattering now validated on cam 0/6/12 (cam 18 + low-σ pending, §8.13),
  no longer cam-0-only; (d) the scattering "look" (albedo/σ) is now a
  design choice, NOT constrained by refs/ (which are absorption — [[reference_asset_density_scales]]).
- **Low-σ interior check — IN PROGRESS (§8.13, P2).** σ=7.5 clamps the dense interior to black on
  both sides, so that match is partly trivial; the σ=2 re-render (gray interior → non-saturated
  transmittance comparison) is running in the P2 batch.
- ~~**uint16 spp ceiling.**~~ RESOLVED (branch `feature/robustness-fixes`): `Image::sample_counts_`
  widened `uint16_t`→`uint32_t` (device POD + host AsyncBuffer + raygen write cast). Welford count no
  longer wraps at 65536 spp; bit-identical ≤65535 spp, furnace PASS. Cost +2 B/pixel.
- ~~**Cap overflow is silent.**~~ PARTIALLY RESOLVED (branch `feature/robustness-fixes`): overflow is
  now DETECTED, not silent. `CompactSet`/`BitVector` `insert()` return a bool; the active-prims and
  primary-ray hit-buffer drop sites bump a device atomic (`report_overflow`) in `LaunchParams`, and the
  host reads it back and WARNS (count + which cap to raise). Verified end-to-end (cap→1 ⇒ ~4.7e7 events
  + warning; cap 128 ⇒ zero on the cloud). NB the fused shadow-ray path is already buffer-free (handles
  unbounded overlap), so only the primary/scatter ray is capped. Still OPEN: true *graceful* unbounded
  overlap for the primary ray (would need argmin without the fixed HitBuffer — wavefront-scale); for now
  the cure remains raising `MAX_ACTIVE_PRIMS`/`HIT_BUFFER_CAPACITY`, but it is no longer silent.
- **AA-floor attribution** (§7) could be nailed by forcing identical pixel filters
  (box on both) or rendering at 2× and downsampling.

---

## 10. Pointers to older notes

- `tools/refs/CONCLUSIONS.md` — the voxel-reference pipeline (stalled; spatial
  convention unknowns). Superseded by the prb-reference approach above.
- `REGRESSION_ANALYSIS.md` — why the April-26 "wispy" render was buggy-but-flattering,
  not a regression.
- `claude-docs/` — older planning notes (denoiser, NEE, quality roadmap, hybrid
  wavefront, cloud-calibration). Historical; verify against current code before use.
