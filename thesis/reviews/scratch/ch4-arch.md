# Ch 4 (Architecture) review — code/math cross-check (2026-06-15)

**Scope:** `thesis/latex/chapters/04-architecture.tex` (primary), cross-checked vs
`device/core/sampling.cuh`, `include/thesis/device/params/primitive.h`, `device/entry/raygen.cuh`,
`device/entry/anyhit.cuh`, `device/core/trace.cuh`, `device/core/hit_record.cuh`,
`device/core/constants.cuh`, `include/thesis/common/utils/math.h`,
`include/thesis/common/geometry/intersection.h`, `src/thesis/host/utils/io/ply.cpp`,
`src/thesis/host/app/config.cpp` + `config.h`, `test/test_runner.cpp`,
`src/thesis/host/app/renderer.cpp`, `src/thesis/host/main.cpp`. Consistency skim of `abstract.tex`,
`chapters/01-introduction.tex`. Regression-checked against `2026-06-10-full-review-ex-ch6.md` §4.

**Headline:** The two new derivations and the argmin description are **correct**. All prior-review §4
MAJOR items (MIS, overflow "handled", Algorithm "remember k", "unnormalised", $K_k$, false runtime
flags) are **fixed** in the current text. The remaining issues are (a) a runtime-flag claim that is
true only for the *experiment-driver* binary, not the `config.cpp` app binary the prose implies; and
(b) a residual "sorted sequence" attribution to the reference that contradicts the corrected framing
elsewhere. No blocking math/algorithm errors.

---

## 1. Blocking

*(None.)* No wrong/unsupported algorithm or math claim, and no internal contradiction severe enough to
block. The closest call is the runtime-flag wording (§2, Should-fix #1) — it is misleading but
defensible for the binary that actually runs the experiments, so it is a Should-fix, not Blocking.

---

## 2. Should-fix

**SF-1 — "runtime flags" is true for the experiment binary, NOT the `config.cpp` app binary the prose
implies.** `04-architecture.tex:436-444`: "the OptiX denoiser (`--denoise`) and volumetric product-RIS
(`--ris`, with candidate count `--ris-candidates`) are *runtime* flags ... The rendering parameters
themselves are likewise exposed on the command line---samples per pixel (`--spp`) ... the density
scale (`--sigma-multiplier`) ... so an experiment is specified entirely by its invocation."
- Reality: `--ris`, `--ris-candidates`, `--spp`, `--sigma-multiplier` are registered **only** in
  `test/test_runner.cpp:108-127` (the experiment driver that loads the PLY assets, runs the cloud/bunny
  scenes, and plumbs `use_ris_` → device via `renderer.cpp:217`). The primary app
  `src/thesis/host/app/config.cpp` (the `main.cpp` binary, which renders only a hardcoded single
  purple Gaussian, `main.cpp:21-30`) does **not** register `--ris`/`--ris-candidates`, and exposes spp
  as `--samples_per_pixel`, not `--spp`, with **no** `--sigma-multiplier` at all (`sigma_multiplier`
  defaults to `7.5f` in `io.h:43`).
- So the claim is **true for `test_runner`** (which is how every experiment in the thesis is actually
  invoked) and **false for `config.cpp`**. This *resolves* the prior review's act-first #3 ("`--ris`
  unreachable at runtime") — it IS reachable, via `test_runner`. But the prose says "the renderer"
  generically and names `--spp`/`--sigma-multiplier`, which only exist in the test binary.
- Fix: scope the sentence to the experiment harness, e.g. "Experiments are driven through a test-runner
  front-end that exposes these as command-line flags (`--ris`, `--ris-candidates`, `--spp`,
  `--sigma-multiplier`, ...)", OR register the same flags in `config.cpp` so "the renderer" is literally
  true. (Code fix is cleaner and matches the prior review's intent; the plumbing already exists in
  `renderer.cpp`.) The adaptive-sampling-/fast-erf-as-compile-time half of the sentence is **correct**
  (`constants.cuh:187` `constexpr ENABLE_ADAPTIVE_SAMPLING`; `math.h:316-320` `THESIS_ENABLE_FAST_ERF`
  CMake gate).

**SF-2 — residual "sorted sequence" attribution to the reference, contradicting the corrected framing
elsewhere.** `04-architecture.tex:7-8`: "The reference~\cite{DSYG} marches the ray segment by segment
through the **sorted sequence** of primitive boundaries and root-finds the scatter distance". The prior
review (act-first #2, verified against `volprim/integrators/common.py`) established that the reference
does **not** sort — it selects the next boundary by a running minimum. The intro (`:7-8` of
`01-introduction.tex`) and this chapter's own `fig:pipeline` caption (`:60-61`: "maintaining the
overlapping-primitive set and selecting the next boundary by a running minimum") get it right, so
line 7 now contradicts them. Fix: "...through the sequence of primitive boundaries (selected by a
running minimum)..." — drop "sorted".

**SF-3 — "exactly" reproduces the combined distribution: scope to the same 3σ-truncated medium the
reference uses.** `04-architecture.tex:189-192` and the proof (`:196-209`) claim the argmin is an
*exact* sample from "the combined medium $\sigma_t=\sum_k\sigma_{t,k}$". The implementation integrates
each Gaussian only over its hard $3\sigma$ BVH bound (`intersection.h:50,59,121`; `optical_depth`
clamps `t_limit` to `GAUSSIAN_DIAMETER_F` = $6\sigma$ arc-length, `primitive.h:282`), i.e. a
$3\sigma$-truncated Gaussian (99.7% of mass). The identity is exact *for that truncated medium*; it is
not the analytically-untruncated Gaussian sum. This truncation is shared with the reference (DSYG also
uses a bounded extent), so the *relative* "reproduces the reference's distribution exactly" claim is
fine — but a one-clause caveat ("for the same $3\sigma$-bounded primitives the reference uses") would
make the "exact" honest. Minor; the proof itself is valid (each per-primitive $\tau_k(t)$ in the proof
is precisely the truncated optical depth the code computes, and "$\tau_k(t)=0$ before entry" holds
exactly under the bound).

**SF-4 — Algorithm 1's `inv_cdf` vs `inv_cdf_segment` split is correct but the pseudocode hides the
two different procedures behind look-alike names.** `alg:argmin` (`:259,266`) calls `inv_cdf` for the
active set (invert from $t_0{=}0$) and `inv_cdf_segment` for hit-buffer entries (invert from
$t_{\text{entry}}$). This matches the code (`sampling.cuh:386` `prim.inv_cdf`, `:403`
`prim.inv_cdf_segment`) and is correct. But the pseudocode omits the FP-undershoot clamp
(`sampling.cuh:413-415`, "$t_{\text{scatter}}<\text{hit\_t}\Rightarrow\text{hit\_t}$") and the
two-stage exit recompute. These are numerical-robustness details, legitimately abstracted away — no
action strictly required, but a single comment "(numerical guards omitted)" on the algorithm would
pre-empt an examiner asking why the code is longer. Optional.

---

## 3. Polish

**P-1 — `:73-74,76,295` `N` overloaded** (carried from prior review NIT, still present). `:73`
"a ray crossing $N$ overlapping primitives" uses $N$ for the per-ray crossed count, while `:295,300`
and `sec:gpu-impl` use $N$ for the scene total (`MAX_PRIMITIVES`, the active-set scan bound). The
`fig:pipeline` "Trace + march ($\times N$)" box (`:42`) repeats the per-ray sense. CONVENTIONS reserves
$H$ for the per-ray hit count. The `:295` complexity line "$O(N + A)$ ... plus an $O(A + H)$ pass" then
silently switches $N$→scene-total mid-paragraph. Pick $H$ for the crossed count in `:73-77` and the
figure box.

**P-2 — `:91` "fast local memory"** (carried from prior review NIT). CUDA *local* memory is off-chip
DRAM; it is "fast" only while L1-resident — which is the chapter's own occupancy/megakernel argument
(`:421-431`). "Per-thread local memory (kept cache-resident)" is more precise and self-consistent.

**P-3 — `:124` "the per-primitive part of which is precomputed"** is correct (only `inv_cdf_factor_` is
precomputed, `primitive.h:84`) but slightly opaque. Could name it: "...the per-primitive factor
$\sigma_{t,k}\,(2\pi)^{-3/2}\!\prod_i s_{ki}^{-1}\sqrt{\pi/2}$ is precomputed (`inv_cdf_factor_`)". Optional.

**P-4 — `:265` "well over an order of magnitude"** is fine; prior review's 25600 vs 652 ≈ 39× note is
about the *bunny-vs-cloud* count in `:412-415` ("`25600`-Gaussian bunny"), which is internally
consistent. No action.

**P-5 — `:254` "want to be as small as is safe"** colloquial for the surrounding register. Optional
tightening to "should be as small as is safe".

---

## 4. Verified-correct (no action) — load-bearing claims checked

**Derivation `sec:analytic-od` (closed-form optical depth) — CORRECT, verified line-for-line vs code:**
- $C_k=\sigma_{t,k}((2\pi)^{3/2}\prod_i s_{ki})^{-1}$ (`:107`) == `optical_thickness_ ·
  density_norm_factor_` where `density_norm_factor_ = (2π)^{-3/2}·∏(1/s)` (`primitive.h:83`).
- Whitening $\mathbf{y}(s)=\mathbf{a}+s\mathbf{b}$, $\mathbf{a}=S^{-1}R^\top(\mathbf{o}-\mu)$,
  $\mathbf{b}=S^{-1}R^\top\omega$ (`:108-109`) == `transform_pos_local`/`transform_dir_local`
  (`primitive.h:98-104`).
- Completing the square: $s^\star=-\frac{\mathbf{a}\cdot\mathbf{b}}{w^2}$,
  $d_\perp^2=\|\mathbf{a}\|^2-\frac{(\mathbf{a}\cdot\mathbf{b})^2}{w^2}$ (`:112-114`) — algebra checks
  out; matches `wp=(a·b)/|b|`, `diff=pp-wp²=d_perp²` (`primitive.h:150-152`).
- $\tau_k(t_0,t_1)=C_k'[\operatorname{erf}(u_1)-\operatorname{erf}(u_0)]$,
  $C_k'=C_k e^{-\frac12 d_\perp^2}\sqrt{\pi/2}\,/w$, $u_i=\frac{w}{\sqrt2}(t_i-s^\star)$ (`:120-122`)
  == `optical_depth` return `optical_thickness_·G_term·e_term·erf_term·density_norm_factor_` with
  `G_term=√(2π)/w`, `erf_term=½[erf((B+t_limit)/√2)−erf(B/√2)]` (`primitive.h:290-307`). The
  $\sqrt{2\pi}/2=\sqrt{\pi/2}$ identity makes the prefactors equal; the erf arguments reduce to
  $u_0=(wp+t_0|b|)/\sqrt2$, $u_1=(wp+t_1|b|)/\sqrt2$, exactly the thesis $u_i$. **Confirmed.**
- Inverse $t(\tau)=s^\star+\frac{\sqrt2}{w}\operatorname{erf}^{-1}(\operatorname{erf}(u_0)+\tau/C_k')$
  (`:130-132`) == `inv_cdf` return `(√2·erfinv(erf(wp/√2)+τ/K)−wp)·w_inv_len` with `K=C_k'`
  (`primitive.h:157-180`). **Confirmed exact.** `fig:optical-depth` (whitened frame, $s^\star$,
  $d_\perp$, $u_0,u_1$, $C_k'[\operatorname{erf}(u_1)-\operatorname{erf}(u_0)]$) matches the derivation.

**Derivation `sec:adt` (argmin exactness) — CORRECT and RIGOROUS:**
- Free-flight CDF $\tau_k=-\ln(1-\xi_k)$, $\xi_k\sim\mathcal{U}[0,1)$ (`:180,258,265`) ==
  `sample_free_flight_tau`: `-log(max(1-χ,1e-10))` (`sampling.cuh:310-313`). **NOT raw $\xi$** — the
  CLAUDE.md "raw χ vs free-flight" question is resolved in favour of $-\ln(1-\xi)$. Confirmed (this is
  SDTracking Theorem 1).
- Proof (`:196-209`): $\Pr[\min_k t_k>t]=\prod_k e^{-\tau_k(t)}=e^{-\sum_k\tau_k(t)}=e^{-\tau(t)}=T(t)$.
  Independence of $\xi_k$ is stated; optical-depth additivity over overlaps is stated; truncation/escape
  ($t_k=+\infty$ when a primitive's mass $<\tau_k$, `primitive.h:174,230` → `INF_F`) is correctly
  absorbed (the product gives total escape prob $e^{-\sum\tau_k^{\max}}$). The "argmin = combined
  free-flight sample" claim **holds**.
- Entered-primitive rejection-bias handling (`:276-282`): the segment-restricted inverse shifts the erf
  reference to $t_{\text{entry}}$ rather than rejecting samples $<t_{\text{entry}}$ — matches
  `inv_cdf_segment` (`primitive.h:198-235`, erf reference `wp_off = fma(t_offset,w_len,wp)`). Correct,
  and the prose correctly flags that naive reject-before-entry would bias (`sampling.cuh:400-403`).
- "No majorant, no null-scattering" (`:206-209`) — TRUE: no majorant constant anywhere, each `inv_cdf`
  is exact `erfinv`, no null-collision rejection loop. The analytic invertibility removes stochastic
  tracking. `fig:argmin` (three overlapping Gaussians, per-primitive $t_k$, $t_{\text{scatter}}=\min_k
  t_k$) matches the algorithm and prose.

**Algorithm 1 (`alg:argmin`) — matches `sample_scattering_event`:** loop over active set (invert from
$t_0{=}0$) then hit buffer (invert from $t_{\text{entry}}$); accept `0≤t_k≤t_exit(k) ∧ t_k<t*`; return
$t^\star$ or **Escape** (`sampling.cuh:435-441`). The prior review's "remember k / nonexistent host
primitive" bug is **gone**: the algorithm returns $t^\star$, and the prose (`:192-194`) correctly
describes (a) albedo as a $\sigma_t$-weighted mixture over containing primitives (`evaluate_albedo`,
`sampling.cuh:257-283`) and (b) the active set rebuilt by a **second linear pass** "no new trace"
(`final_active_prims`, `sampling.cuh:444-475`). **Confirmed.**

**§4.2/4.3 density + σ_t convention — CORRECT (the "unnormalised" wording is gone):**
- The chapter never says "unnormalised"; `:105-107` quotes the $((2\pi)^{3/2}\prod s_i)^{-1}$
  normalisation, which the code applies (`density_norm_factor_`, used in `pdf` `primitive.h:134` and
  `optical_depth` `:307`). This is a **normalised** convention, stated correctly.
- σ_t / mass convention vs Mitsuba: the PLY `sigma_t` is the per-primitive total integrated mass,
  scaled by `sigma_multiplier`, with **no** $(2\pi)^{3/2}\prod s$ bridge (`ply.cpp:107-118`) — matching
  Mitsuba's `volprim_tomography` `GaussianKernel.density_integral`. The chapter does not over-specify
  this (it lives in the data/IO discussion), consistent.

**Single-trace collection + analytic exits — CORRECT:** any-hit with `DISABLE_CLOSESTHIT |
CULL_BACK_FACING_TRIANGLES` (`trace.cuh:36-37,72-73`); entries-only append (`anyhit.cuh:76-85`); exits
computed in closed form, not traced (`sampling.cuh:382,427`, `exit_from_inside`/
`compute_exit_from_entry`); `HitBufferSoA` 4 B (`t_hit_`) + 2 B (`prim_idx_`) (`hit_record.cuh:27-28`),
matching `:90`. Active set + hit buffer both feed the sampler (`:94-98`). **Confirmed.**

**Overflow framing — HONEST (regression fixed):** `:92` "overflow is *detected* rather than
corrected---a dropped hit biases the pixel toward under-absorption, but a device counter flags the
event"; `:419` "excess overlap is *detected* rather than silently corrupting the image". Matches
`anyhit.cuh:80-84` (drop excess hit, keep traversing for the env background, `report_overflow`). No
"handled"/"corrected" overclaim remains. **Confirmed.**

**MIS — CORRECT (regression fixed):** `:308-314` "two shadow connections are drawn per vertex---one
sampling the environment by luminance, one sampling the Henyey--Greenstein phase function---each
weighted by the balance heuristic ... Indirect light is carried by the phase-sampled continuation ray,
whose direct environment contribution is suppressed so it is not double-counted." Matches
`raygen.cuh:241-260` (Strategy A phase-IS traced + Strategy B env-IS traced, both `mis_balance`) and
`raygen.cuh:169-177` (escape env suppressed for bounce>0 under NEE). `constants.cuh:152` confirms "2×
shadow rays per scatter". **Confirmed — this was the prior review's top Ch 4 MAJOR; now correct.**

**RIS one-shadow-ray description — CORRECT:** `:315-316` "an optional resampled-importance-sampling
variant that traces a single shadow ray, keeping the MIS scaffold intact." Matches
`raygen.cuh:200-240` (K env-IS candidates → 1-survivor weighted reservoir → only survivor's
transmittance traced). **Confirmed.**

**Rao–Blackwell direct term — CORRECT:** `:319-323` "adds the analytic camera-to-environment term
$T_{\text{cam}}L_{\text{env}}$ at bounce zero ... for the cost of a single transmittance ray." Matches
`raygen.cuh:139-150` (`ENABLE_ANALYTIC_DIRECT`, `compute_transmittance_to_env` at bounce 0). The "only
lower variance" / conditional-expectation framing is sound and the prior "at no extra trace" overclaim
is fixed (it now says "for the cost of a single transmittance ray"). **Confirmed.**

**Russian roulette — CORRECT:** `:323-325` "survival probability the throughput's largest channel
clamped to 0.99". Matches `raygen.cuh:280-289` `min(rr_max_survival_, max(throughput))` with
`rr_max_survival_` default 0.99 (`constants.cuh:128`), beyond `rr_depth_` ("configurable depth",
`--rr-depth`). **Confirmed.**

**Welford accumulation — CORRECT:** `:327-330` single-pass online mean (+variance when adaptive on).
Matches `raygen.cuh:331-340` (M2 update guarded by `if constexpr ENABLE_ADAPTIVE_SAMPLING`). **Confirmed.**

**GPU config — CORRECT:** sphere-GAS (built-in `OPTIX_BUILD_INPUT_TYPE_SPHERES`) + per-primitive IAS
instances; both build flags `PREFER_FAST_TRACE` + `ALLOW_COMPACTION` (`:335-348`); BitVector ≤256 /
CompactSet >256 split (`:350-355` == `constants.cuh:22-26`, `sampling.cuh:32-34`); megakernel rationale
(`:421-431`); `fig:per-ray-state` (hit buffer H×6 B + active set A×2 B, per-resident-thread VRAM floor)
matches `hit_record.cuh` + `constants.cuh`. The built-in-sphere "does not run on RT cores" caveat
(`:338-339`) is an honest, correct hardware note. **Confirmed.**

**Novelty / attribution — CORRECT and appropriately hedged:** `:284-293` credits the
minimum-of-free-flights identity to decomposition tracking (`Kutz2017`), the closed-form Gaussian
optical depth to the reference (`DSYG`), claims only the *synthesis*, and states "The approach was
suggested by Condor, a co-author of the reference---a direction its authors anticipated but had not
implemented." This matches the project record and CLAUDE.md (Condor proposed the argmin). Intro
(`01-introduction.tex:30-33`) and abstract (`abstract.tex:20-21`) mirror this ("an approach the
reference's authors anticipated but did not implement") — consistent, no unattributed-novelty overclaim.
*(Caveat the prior review raised: verify the `\cite{DSYG}` at `:291` is the right pointer if the cite
is meant to mean "personal communication" rather than "the paper anticipates ADT-argmin". Out of scope
for this pass — flagged there, unchanged here.)*

---

## 5. Prior-review §4 regression summary (all addressed)

| Prior §4 item | Status in current text |
|---|---|
| MAJOR MIS (1 vs 2 shadow rays) | **FIXED** (`:308-314`) |
| MAJOR overflow "handled"/"corrected" | **FIXED** (`:92,419`) |
| MAJOR Algorithm "remember k"/host primitive | **FIXED** (Alg 1 returns $t^\star$; mixture albedo + 2nd pass `:192-194`) |
| MAJOR false "runtime flags" (3 of 4) | **FIXED for adaptive/fast-erf** (now "compile-time"); RIS now registered in `test_runner` + plumbed — see SF-1 nuance |
| MINOR "$K_k$ precomputed constant" | **FIXED** (`C_k'` per-ray, `:124`) |
| MINOR "unnormalised convention" | **FIXED** (no "unnormalised"; `:105-107` states the normalisation) |
| MINOR Rao–Blackwell "no extra trace" | **FIXED** ("cost of a single transmittance ray", `:323`) |
| MINOR caps "slowed several-fold" | **REWRITTEN** asset-specific (`:363-370`) |
| act-first #2 residual "sorting" in Ch 4 | **RESIDUAL at `:7`** ("sorted sequence") — see SF-2 |
