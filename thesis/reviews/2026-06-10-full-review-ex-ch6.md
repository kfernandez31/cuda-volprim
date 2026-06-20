# Thesis review — all chapters except Ch 6 (2026-06-10)

**Scope:** abstract, acknowledgements, `thesis.tex` front matter, Ch 1–5, Ch 7 (stub), Ch 8,
Appendix A, and the bibliography entries they cite. Ch 6 excluded (WIP; has its own queued rework
list in `docs/superpowers/HANDOFF.md`). Where a finding must flow into the Ch 6 rework, it is marked
**[→ Ch 6 pass]**.

**Method:** every factual claim checked against `thesis/FINDINGS.md` (the lab notebook), the code
(`device/`, `include/`, `src/`, `scripts/`), the source PDFs in `thesis/papers/`, and
`thesis/latex/CONVENTIONS.md`. Six independent per-chapter fact-check passes plus one cross-chapter
consistency pass. Build verified: `latexmk` clean, zero errors, no undefined/duplicate references.

**Severity:** CRITICAL = an examiner with the artifacts would call it overclaiming or a contradiction;
MAJOR = factually wrong or misleading, needs fixing before submission; MINOR = imprecise/incomplete;
NIT = polish.

---

## 0. Act-first list (the short version)

1. **[CRITICAL] Disclose the overlap-scatter residual in Ch 5** and repair the "unbiasedness proof"
   framing (§5 below, C1/C2). The chapter cites the exact rungs that carry the one open residual as
   proof the argmin sampler is unbiased.
2. **[CRITICAL] Purge the residual "sorting" claims.** Commit `38c3d25` corrected the abstract's first
   paragraph, Fig 4.1, and §4.4 — but "boundary-sorting" survives in `abstract.tex:22`,
   `03-related-work.tex:54-57` and `:76-77`, and `08-conclusion.tex:17-18`. The reference does not
   sort (verified against `volprim/integrators/common.py`: marching + running-min boundary selection
   + root-find). These now contradict the corrected Ch 4.
3. **[MAJOR, CODE] `--ris` is not registered in the CLI.** `Config::use_ris_` exists
   (`include/thesis/host/app/config.h:45`) and is plumbed to the device
   (`src/thesis/host/app/renderer.cpp:207` → `raygen.cuh:200`), but
   `src/thesis/host/app/config.cpp` registers no `--ris` / `--ris-candidates` option — RIS is
   unreachable at runtime. Ch 4 §4.7 and Ch 6 ("shipped behind a runtime flag") are false in the
   current binary. Fix the code (likely a lost registration), not the prose. **[→ Ch 6 pass]**
4. **[MAJOR] Ch 4 misdescribes the MIS structure** (one shadow ray + continuation vs the code's two
   shadow rays). §4 below, finding 4.1. Ch 6:93–94 repeats it. **[→ Ch 6 pass]**
5. **[MAJOR] Ch 5 attribution errors:** cloud absorption was validated against *Mitsuba* across 24
   views (analytic only adjudicated the edge band, single view); scattering view-independence comes
   from §8.13/showcase (4 views), not the ladder (cam 0 only); backward HG was never validated
   post-fix. §5 below, M1/M2/M4.
6. **[MAJOR] Appendix A contradicts FINDINGS §8.27/§8.32 and Ch 6's own ledger** (premise
   inverted; track-length hybrid presented as open although investigated and closed). Fold these
   fixes into the planned appendix→Ch 6 merge. **[→ Ch 6 pass]**
7. **[MAJOR] Abstract + Ch 1 overclaim in three places:** unqualified "novel" architecture (no Condor
   attribution — Ch 4 gets this right), unqualified RIS "win" (scene-dependent per §8.37), and
   "measured across … memory" (memory is unmeasured until the campaign's R6/R7).
8. **[MAJOR] DSYG citation:** prose says "ACM SIGGRAPH 2025"; `refs.bib` has only the 2024 arXiv
   `@misc`. The published TOG version (doi 10.1145/3711853, visible on `thesis/papers/DSYG.pdf`)
   should be cited. Same arXiv-only problem for 3DGS, NeRF, MipNeRF, PixelNeRF.

Cross-cutting mechanical passes (do once, thesis-wide): notation collisions (§9.1), `\Cref` vs
`\cref` (§9.2), tense for measured results (§9.3).

---

## 1. Front matter (`thesis.tex`, `abstract.tex`, `acknowledgements.tex`)

- **[MAJOR]** `thesis.tex:75` — "Prof.\ Dr.\ P.\ K.\ Didyk": the DSYG author list and the
  acknowledgements both say "Piotr Didyk"; the "K." initial appears nowhere else. Verify the name.
- **[CRITICAL — part of act-first #2]** `abstract.tex:22` — "segment-marching, boundary-sorting, and
  root-finding outright": drop "boundary-sorting" (the first paragraph was already corrected by
  `38c3d25`; this sentence was missed).
- **[MAJOR]** `abstract.tex:17-18` — "organised around a novel rendering architecture": the
  argmin/ADT approach was proposed by Condor (commit `116b318`; Ch 4 §4.4 credits this correctly).
  The defensible claim is the synthesis / first realisation for this representation. Mirror Ch 4's
  framing.
- **[MAJOR]** `abstract.tex:27-28` — "measured across time, image quality, and memory": no memory
  measurements exist in FINDINGS; R6 (footprint breakdown) and R7 (peak VRAM) are Ch 7 campaign
  TODOs. Either run them first or drop "memory" until they land.
- **[MINOR]** `abstract.tex:28-29` — "a second algorithmic win---volumetric product-resampled direct
  lighting": FINDINGS §8.37 is explicit that the win is scene-dependent (~1.4× on env-maps, ~2.5×
  *worse* on flat env, default OFF) and not novel (Talbot 2005/ReSTIR; the contribution is adaptation
  + measurement). Qualify: "a second, scene-dependent win — product-resampled direct lighting,
  \(1.4\times\) at equal quality under environment lighting".
- **[MINOR]** `abstract.tex:30-32` — "Together these close an initial roughly fivefold equal-quality
  deficit … and overtake it on the showcase scene":
  - The ~5.5× deficit (§8.5) was measured on flat/constant lighting only; on the env-lit showcase the
    renderer already won pre-optimisation (§8.15). Scope the claim.
  - The antecedent of "these" is wrong: what closed the gap was the §8.16 shadow-ray transmittance
    optimisation (~12–15× kernel speedup) — the single largest result in the thesis, currently
    invisible in the abstract. Consider naming it.
  - Tense: "close … overtake" reports measured results; CONVENTIONS mandates past tense
    ("closed … and overtook").
- **[NIT]** `abstract.tex:10-11` — ambiguous "its" in "the reference renderer that established its
  correctness".
- **[NIT]** `abstract.tex:29` — "rigorously profiled" → "profiled" (filler per CONVENTIONS).
- **Verified, no action:** "Luxembourg, June 2026" was set deliberately (commit `116b318`).
  Acknowledgements (Didyk, Condor, Rybicki, family) are in place and consistent with the handoff
  decision.

## 2. Chapter 1 — Introduction

- **[MAJOR]** `:72-74` — contributions bullet claims "a novel single-trace … rendering architecture"
  with no attribution; same fix as the abstract (e.g. "…realising an approach proposed by the
  reference's authors but not previously implemented").
- **[MAJOR]** `:55` — "presented at ACM SIGGRAPH 2025 by Condor et al." vs `refs.bib` arXiv-2024
  `@misc`. Update the bib entry to the published TOG version (doi 10.1145/3711853) or drop the venue
  claim. (Act-first #8.)
- **[MINOR]** `:80-81` — "a second algorithmic win (volumetric product-RIS)": add the
  scene-dependent qualifier (see abstract item).
- **[MINOR]** `:81-82` — "measured across time, quality, and memory": same memory caveat as the
  abstract.
- **[MINOR]** `:82,88` — the intro promises `\Cref{ch:results}` delivers the performance results;
  Ch 7 is a 14-line scaffold. Not a writing error (tracked dependency on the campaign), but the
  abstract's quantitative claims have no in-thesis home until Ch 7 lands. Tracked already in HANDOFF.
- **[NIT]** `:1-42` — template boilerplate (commented sample sections, TeX spacing tutorial) and the
  unused `\newcommand{\package}{\emph}` at `:2`. Delete.
- **[NIT]** `:81` — "RIS" first used unexpanded in the body (the abstract is a separate document);
  expand at first use.

## 3. Chapter 2 — Background

No critical findings. Core math verified correct: MC estimator, rendering equation,
Beer–Lambert/optical depth, free-flight inversion τ(t) = −ln(1−ξ) with ξ∈[0,1), HG normalisation
(checked by integration), balance heuristic, unnormalised-Gaussian-with-w_k convention, erf closed
form. All 17 cited keys exist in `refs.bib`.

- **[MAJOR]** `:148` — mixture written as $\sum_{k=1}^{K}$: $K$ collides with the thesis-wide $K$ =
  RIS candidates (CONVENTIONS; `06-optimization.tex:111` uses $K=6$). The kernels *are* the scene
  primitives, which Ch 4 calls $N$. Change the count to $N$ (index $k$ is fine).
- **[MINOR]** `:31` — "because error scales with $\sqrt{N}$, halving the noise costs four times the
  samples": error scales as $1/\sqrt{N}$. Conclusion right, clause literally wrong.
- **[MINOR]** `:23-25` — unbiasedness + $O(1/N)$ variance attached to the support condition alone;
  the variance claim additionally needs $\int f^2/p < \infty$. Add "provided the variance of $f/p$
  is finite".
- **[MINOR]** `:87-88` — "τ … must be estimated stochastically, for instance by delta tracking":
  delta tracking samples free flights / estimates transmittance $T=e^{-\tau}$, not τ (and $-\ln$ of
  an unbiased $T$ estimator is a biased τ estimator). Rephrase in terms of $T$.
- **[MINOR]** `:134` — balance heuristic uses $n_s, n_k$ without defining them (sample counts per
  strategy). One clause fixes it.
- **[MINOR]** `:137-138` — "provably keeps the combined estimator's variance close to that of the
  best individual strategy": Veach & Guibas bound the balance heuristic against the best *weighting
  combination*, within an additive term — not against the best single strategy. Restate precisely;
  examiners press on this one.
- **[MINOR]** `:116-117` — θ in $p_{\mathrm{HG}}(\cos\theta)$ never defined (angle between incident
  propagation and outgoing direction); the sign convention for $g$ depends on it.
- **[MINOR]** `:119-121` — physically dubious examples: haze is strongly forward-scattering
  (poor isotropic example); atmospheric aerosols are a poor $g<0$ example. Use neutral examples
  (isotropic as idealisation / high-order limit; backscattering media for $g<0$).
- **[MINOR]** `:157-159` — "the same anisotropic 3D Gaussians *introduced by* 3D Gaussian Splatting":
  anisotropic Gaussian scene primitives predate 3DGS (EWA splatting, Zwicker et al. 2001/02).
  "Popularised by" / "as used in".
- **[MINOR]** `:197` — BVH cited to MacDonald & Booth 1990, which is the kd-tree/space-subdivision
  SAH paper. Canonical BVH cites: Rubin & Whitted 1980, Kay & Kajiya 1986, Goldsmith & Salmon 1987
  (none in refs.bib). Keep MacDonald & Booth only if SAH build heuristics are meant.
- **[MINOR]** `:20` vs `:195` — $N$ = MC sample count and $N$ = primitive count in the same chapter.
  Use a different symbol (or "spp") for sample count. (See §9.1.)
- **[NIT]** `:30-31` — zero variance at $p \propto f$ holds for single-signed $f$; add the half-clause.
- **[NIT]** `:113/132` vs `:18` — $p$ does triple duty (sampling density, phase function, strategy
  densities); consider $f_p$ or a subscript for the phase function.
- **[NIT]** `:196` — "$O(N)$ to $O(\log N)$": typical, not worst case; "typically".
- **[NIT]** `:155` — $w_k$ glossed as "(its density)"; it is a peak-extinction amplitude/weight.
- **[NIT]** `:185` — Epanechnikov "often improving both speed and fidelity" is stronger than DSYG's
  claim (efficiency, in reconstruction). Soften or scope.
- **[NIT]** `refs.bib:97` — `SpatialAccelerationStructures` is a lecture-slides URL; weak next to the
  1990 paper. Replacing it with the canonical BVH papers also fixes the item above.

## 4. Chapter 4 — Architecture (vs the code)

Verified correct against the implementation (no action): single any-hit trace with
`DISABLE_CLOSESTHIT | CULL_BACK_FACING_TRIANGLES` (`device/core/trace.cuh:36-37`); entries-only
collection + analytic exits (`device/entry/anyhit.cuh:54-70`, `sampling.cuh:374,407`); HitBufferSoA
4 B + 2 B (`hit_record.cuh:27-28`); BitVector ≤256 / CompactSet split (`constants.cuh:22-26`);
free-flight τ = −ln(1−ξ) (`sampling.cuh:310-313` — the χ-vs-free-flight question in CLAUDE.md is
resolved in favour of the free-flight CDF; CLAUDE.md is stale); segment-restricted inverse + bias
rationale (`primitive.h:183-198`); no escape-case segment fallback exists and the chapter rightly
doesn't claim one (CLAUDE.md stale again); cloud ≈45/96 and bunny 245/387 match `caps_table.csv` and
`tab:overlap`; zero overflows on the cloud (FINDINGS:1623); sphere-GAS/IAS + both build flags;
megakernel; Welford; RR clamp `min(0.99, max(throughput))` (`raygen.cuh:280`, `constants.cuh:128`);
OptiX 9 matches the build config; Algorithm 1's loop structure and acceptance conditions match
`sample_scattering_event`.

- **[MAJOR]** `:209-213` — MIS misdescribed: prose says one luminance-sampled shadow connection +
  the HG continuation, balance-weighted. The code does **two shadow connections** — one
  phase-sampled, one env-sampled, each with its own transmittance (`raygen.cuh:241-260`;
  `constants.cuh:152` "2× shadow rays per scatter") — and the continuation is a separate third phase
  draw whose env hits are suppressed under NEE (`raygen.cuh:169-177`). `06-optimization.tex:93-94`
  repeats the error. **[→ Ch 6 pass]**
- **[MAJOR]** `:92-93, :268-270` — "overflow is detected and handled rather than left to corrupt the
  result": on overflow the result *is* corrupted (dropped hits → under-absorption → too bright); it
  is detected, not corrected (`anyhit.cuh:60-69`; estimate_caps.py docstring; FINDINGS:944-949).
  State the honest version — Ch 4 said it correctly elsewhere per the handoff; this sentence
  regressed it.
- **[MAJOR]** `:148-149` + Algorithm 1 (`:157,163,170,174`) — "host primitive … fall out of the same
  pass" / "remember k": the kernel never records the argmin winner — only `t_scatter_min`
  (`sampling.cuh:367-413`); albedo is a σ_t-weighted mixture over all primitives containing the
  scatter point (`evaluate_albedo`, `sampling.cuh:257-283`), and the active set is rebuilt in a
  **second** pass (`sampling.cuh:423-455`). Fix the algorithm (return t* or Escape) and the prose
  (mixture albedo; second linear pass, no new trace).
- **[MAJOR]** `:286-288` — "exposed as runtime flags" is false for 3 of 4 listed features: adaptive
  sampling is `constexpr` (`constants.cuh:187`), fast-math erf is a CMake define
  (`math.h:316-320`), and `--ris` is not registered in the CLI (act-first #3). Only `--denoise` is
  runtime (`config.cpp:39`). Fix the code for RIS; fix the prose for the other two.
- **[MINOR]** `:118-120` — "$K_k$ a precomputed per-primitive constant": in code K depends on the
  ray (whitened direction, perpendicular distance); only `inv_cdf_factor_` is precomputed
  (`primitive.h:84,157`). "A per-ray constant built from a precomputed per-primitive factor."
- **[MINOR]** `:111-114` — "unnormalised convention" contradicts the factor it quotes: the
  $((2\pi)^{3/2}\prod s_i)^{-1}$ factor *is* the normalisation, and the code applies it
  (`density_norm_factor_`, `primitive.h:83`, used in `pdf` and `optical_depth`). Either drop
  "unnormalised" or correct the sentence. (CLAUDE.md's "unnormalised convention" note appears stale
  vs current code — verify which is true and fix the other.)
- **[MINOR]** `:220-223` — Rao–Blackwellised bounce-0 term "at no extra trace": it issues an extra
  transmittance-mode `optixTrace` (`raygen.cuh:147-149` → `trace.cuh:58-81`). It saves variance, not
  a trace.
- **[MINOR]** `:255-256` — "raising it well beyond need slowed rendering several-fold": the ~6× was
  measured on the sparse toy cloud; on dense assets raising caps was ~free (FINDINGS:947-949).
  Qualify as asset-dependent.
- **[MINOR]** `:192-194` — "The approach was proposed by Condor~\cite{DSYG}": the project record
  describes an advisor suggestion + reference code (`primitive.h:173` cites `jorge_python.py`), not
  necessarily the paper. If the DSYG paper doesn't itself anticipate the ADT-argmin scheme, the cite
  is a misattribution — "(J. Condor, personal communication)" or scope the cite to what the paper
  actually says. Verify against the PDF before submission.
- **[NIT]** `:73-74` vs `:196` — $N$ overloaded (ray-crossed count vs scene total; the former is $H$
  per CONVENTIONS); the `fig:pipeline` "Trace + march ($\times N$)" box repeats it; and the `:196`
  headline "O(N + A)" silently absorbs H although the next sentence counts it.
- **[NIT]** `:91` — "fast local memory": CUDA local memory is off-chip; it is fast only while
  L1-resident — which is exactly the chapter's own occupancy argument. "Per-thread local memory,
  sized to stay cache-resident."
- **[NIT]** `:265-266` — 25 600 vs 652 is ~39×: "well over an order of magnitude" or "forty times".
- **[NIT]** `:254` — "want to be as small as is safe" is colloquial for the stated voice.

## 5. Chapter 5 — Validation

- **[CRITICAL] C1 — the overlap-scatter residual is undisclosed, and the chapter cites the affected
  rungs as the unbiasedness proof.** `:129-133` + `fig:scattering-ladder` caption: "Single and
  cluster rungs match within noise … empirical evidence that the argmin scatter sampler is
  unbiased." FINDINGS §8.3: the traits cluster rung is "PASS, with one isolated overlap residual" —
  a convergence-stable, genuine +0.0002 core bias (~6 SEM), explicitly OPEN, with the ADT argmin
  distribution under overlap named as a candidate cause; §8.4 finds its echo at cloud scale
  (+1.0e-4 at 3.3σ in the densest core); §9 lists it as open item (a). Ch 8 discloses only the
  coloured-albedo residual, so this one is undisclosed thesis-wide. Fix by stating it as FINDINGS
  does and magnitude-bounding it (≤0.16% peak, confined to overlapped scatter vertices), and carry
  it into Ch 8's limitations. Done right, this *strengthens* the chapter — the methodology resolves
  10⁻⁴-level effects.
- **[CRITICAL] C2 — the intro's unbiasedness logic is unsound.** `:14-16`: "were the argmin rule
  biased, the discrepancy would appear first on the scenes with a known analytic answer" — the
  analytic scenes are the absorption rungs, which never exercise the argmin sampler (albedo 0). The
  only analytic scattering check is the furnace. Re-scope to "the furnace invariant and the
  scattering ladder".
- **[MAJOR] M1** `:100-102` — "The full 652-primitive cloud, validated against the analytic
  absorption ground truth across all camera views": FINDINGS §7 — the 24-view result is CUDA vs
  Mitsuba (`volprim_prb`); the float64 brute-force analytic reference adjudicated only the
  single-view edge band. Reword, or actually run the BF reference over all views first.
- **[MAJOR] M2** `:132` — "the cloud rung passes view-independently": the ladder's cloud rung ran on
  cam 0 only (§8.4); view-independence came from §8.13 (cams 0/6/12/18) in the showcase
  configuration. Move the claim to §5.7 and scope it ("across four well-separated views"). §8.13's
  low-σ +1.8e-4 (21σ) systematic is also OPEN and undisclosed — disclose or drop.
- **[MAJOR] M3** `:108-110` — `fig:absorption-ladder` caption promises per-scene RMSE vs the
  closed-form reference for single/pair/cloud; recorded closed-form numbers exist only for the
  single Gaussian and the collinear test — pair and cloud numbers on record are vs Mitsuba. Run the
  BF tool for those rungs or re-caption per-rung.
- **[MAJOR] M4** `:148-149` — "Henyey–Greenstein … across forward and backward scattering": backward
  (g<0) was never validated post-fix; g=−0.85 appears only as the pre-fix sign-flip diagnostic
  (§8.9). Say "forward scattering (g=0.85)" or run the backward rung.
- **[MINOR]** `:96-98` — Mitsuba's stacked-primitive under-absorption stated without FINDINGS §6's
  caveat (degenerate collinear configuration; did not affect the cloud). Add the qualifier.
- **[MINOR]** `:86` — "matches … to within floating-point tolerance" overstates (record: ~10⁻⁵−10⁻⁶,
  jitter-limited). The next sentence already gives the honest framing; align them.
- **[MINOR]** `:144-157` — feature validation hides that the ladder caught three real bugs (env
  vertical flip, HG sign, `phase::eval` sign — §8.6/8.9/8.10), presenting the orientation lesson
  hypothetically. The absorption section disclosed its cap bug and reads stronger for it. Recommend
  disclosing — it is the best sensitivity evidence the methodology has.
- **[MINOR]** `:159` — "With every feature correct in isolation and in combination" contradicts
  `:150-151` (the coloured-albedo exception just stated). "With the features validated to within the
  stated residuals…"
- **[MINOR]** `:65` — $k=\mathrm{RMSE}^2\cdot N$: FINDINGS defines k via RMSE² = k²/spp, so the
  thesis k is FINDINGS' k². Self-consistent so far, but anyone transcribing kC=0.411 into a Ch 7
  table would be wrong by a square. Pick one convention and note it when campaign numbers land.
  Also: $N$ here is sample count (see §9.1).
- **[NIT]** `:97` — straight quotes around "differential" (use ``...''). Only occurrence.
- **Verified, no action:** setup (3090/8.6/24 GB, OptiX 9, CUDA backend, full-clock operating
  point); furnace test framing incl. Mitsuba NEE +6.5% and analog-as-ground-truth (exactly §8.1);
  cap-overflow story incl. $\exp(-\min(\text{cap},K)/K\cdot\tau_0)$ (§6); 652 primitives; albedo-0.9
  ladder; showcase config; bias/variance decomposition methodology (§0/§8.0); all labels resolve.

## 6. Chapter 7 — Results (stub)

Verified: scaffold matches design-spec §9's R-set exactly (R1–R7, asset list, full-blast operating
point). No findings. Gate: the experiment campaign. Note act-first #7's dependency — abstract/intro
quantitative claims (fivefold, overtake, firefly-free, memory) have no in-thesis home until this
chapter lands.

## 7. Chapter 8 — Conclusion

- **[CRITICAL — part of act-first #2]** `:17-18` — "replace the segment-marching, boundary-sorting,
  and root-finding": drop "boundary-sorting" (38c3d25 didn't touch this file).
- **[MAJOR]** `:21-22` — "a second algorithmic win (volumetric product-RIS direct lighting)" with no
  scene-dependence qualifier (§8.37: ~1.4× env-maps, ~2.5× worse flat, default off). Ch 6 qualifies
  it correctly; the conclusion must too.
- **[NIT]** `:23` — "$\sim\!5\times$": FINDINGS §8.5 measured ~5.5×; Ch 6 rounds the same way, so it
  is internally consistent, but "~5.5×" would be tighter in both. **[→ Ch 6 pass]**
- **Verified, no action:** contributions recap matches Ch 4/5/6; all six limitations check out
  against the record (incl. the cap-estimator description — the HANDOFF "not yet written" note is
  stale, `scripts/tools/estimate_caps.py` exists; bunny/cloud numbers match `tab:overlap`); future
  work is genuinely open (no proven dead end appears); all cited keys exist.

## 8. Appendix A — A1 (folding into Ch 6; fix during the merge) **[→ Ch 6 pass]**

- **[MAJOR]** `:4-5` — "the hypothesis it began with was wrong, the corrected diagnosis identified a
  real variance lever": inverts FINDINGS §8.27 ("The ORIGINAL §8.5 premise … was RIGHT"); what was
  wrong was the intermediate dismissal. The appendix's own diagnosis (`:63-65`) says so — internal
  contradiction within the same file. Rephrase: first verdict dismissed the hypothesis; the
  re-investigation overturned the dismissal and confirmed the original premise.
- **[MAJOR]** `:85-87` — "folding transmittance into the throughput at every bounce means abandoning
  the binary scatter/escape decision that *is* the sort-free argmin scheme": FINDINGS §8.32
  explicitly corrected this as an overstatement (track-length and argmin are NOT architecturally
  exclusive). Also contradicts the appendix's own closing sentence. Replace with §8.32's actual
  reasons (the prize is already captured analytically at every NEE vertex; the rewrite targets a
  non-showcase regime).
- **[MAJOR]** `:89-92` — the track-length hybrid presented as "the open direction": §8.32
  investigated exactly this and closed it ("dead end for the scattering showcase, confirmed three
  independent ways"); Ch 6's ledger (`06-optimization.tex:177-178`) records the closure, so appendix
  and Ch 6 currently disagree — fatal once the appendix folds into Ch 6. Cite the §8.32 outcome and
  scope any residual openness explicitly to the flat-environment regime.
- **[MINOR]** `:63-64` — "exactly as the depth-1 row … shows": the table's depth-1 row (0.00599,
  1.60×) supports only the weaker claim (gap smallest at depth 1, opens with depth); the ≈0-noise
  observation belongs to the weighted-analog diagnostic column not reproduced here. Soften.
- **[NIT]** `:50` — "reproduces the full gap": single-Gaussian gap (5.0×) is larger than the
  flat-env cloud gap (~3×, §8.15); FINDINGS' own "reproduces the gap" is more precise.
- **Verified, no action:** all numbers match FINDINGS App A exactly (2.85×, depth table, six seeds,
  MIS-off 1.06×, "natural fix strictly worse" argument, 0.5% footnote).

## 9. Thesis-wide mechanical passes

### 9.1 Notation collisions (one sweep, guided by CONVENTIONS: N=primitives, A=active, H=hits, K=RIS, spp)
- `02-background.tex:148` — mixture count $K$ → $N$.
- `02-background.tex:20` — MC sample count $N$ → different symbol/spp.
- `04-architecture.tex:73-74` + `fig:pipeline` — ray-crossed "N" → $H$.
- `05-validation.tex:65` — $k=\mathrm{RMSE}^2\cdot N$ — sample count symbol + k-vs-k² convention.
- `05-validation.tex:93-95` — collinear stack count $K$ → another symbol (e.g. $m$).
- `04-architecture.tex:118` — $K_k$ inverse-CDF constant: rename if it reads as RIS-K (e.g. $C_k$).

### 9.2 `\Cref` vs `\cref`
CONVENTIONS mandates `\cref` mid-sentence; the entire thesis uses `\Cref` everywhere (`\cref` never
appears). Decide once: either a global mid-sentence `\cref` sweep, or amend CONVENTIONS.md to match
practice. (Capitalised "Chapter 4" mid-sentence is a defensible house style; just make the doc and
the text agree.)

### 9.3 Tense for measured results
CONVENTIONS reserves past tense for what was measured. Violations cluster in the abstract
("close … overtake") and Ch 5 ("the renderer matches", "passes this furnace test" — pervasive,
arguably a deliberate timeless-property voice, but it conflicts with the written rule and with
§5.1's own past tense). Decide the rule, then sweep.

### 9.4 Bibliography upgrades (one bibtex session)
- `DSYG` → published ACM TOG 2025 version (doi 10.1145/3711853); fixes the Ch 1 venue claim.
- `3DGS` → ACM TOG 42(4) 2023; `NeRF` → ECCV 2020; `MipNeRF` → ICCV 2021; `PixelNeRF` → CVPR 2021.
- BVH canonical cites (Rubin & Whitted 1980 / Kay & Kajiya 1986 / Goldsmith & Salmon 1987) replacing
  or joining `MacDonaldBooth1990` + the lecture-slides `SpatialAccelerationStructures`.
- EWA splatting (Zwicker et al. 2001) if the Ch 3 EWA mention stays.
- `Woodcock1965` — add the proceedings title (ANL-7050) for tightness.
- Handoff item 11 (spot-check deliberately omitted DOIs on classics) still open.

### 9.5 Stale project docs (not the thesis, but they misled this review and will mislead the next)
`CLAUDE.md` is stale in at least four places: status line ("renders incorrect, under active
investigation" — long resolved), the escape-case "segment-by-segment fallback" (no longer exists in
code), the open χ-vs-free-flight question (resolved: code uses −ln(1−ξ)), and the "unnormalised
Gaussian convention" note (code applies `density_norm_factor_`; see §4 finding on `:111-114`).
`HANDOFF.md`'s "cap-estimation script NOT yet written" is also stale (it exists). Update both.

## 10. Chapter 3 — Related Work
(placed last because most items overlap act-first #2)

- **[CRITICAL — act-first #2]** `:54-57` — "marching the ray through the **sorted sequence** of
  primitive boundaries … the **sort it depends on** … are precisely what \Cref{ch:architecture}
  removes": the reference does not sort (verified against `volprim/integrators/common.py`:
  running-min selection). Mirror the 38c3d25 phrasing used in Ch 4.
- **[MAJOR]** `:28-30` + `fig:flicker` caption — popping/flicker attributed to the EWA projection
  approximation; the documented cause is the view-dependent **global sort** (StochasticSplats §1;
  DSYG p.1:2) — and `:84-85` says so, contradicting `:28-30` within the chapter. Attribute popping
  to the sort (which also strengthens the §3.4 sort-free link); keep EWA approximation error as a
  separate second cost if desired.
- **[MINOR]** `:76-77` — "no marching, sorting, or root-finding": drop "sorting" (same residual).
- **[MINOR]** `:67-70` — "Spectral and decomposition tracking~\cite{Kutz2017} … splitting the medium
  into a control component … and a residual handled by **weighted** tracking": (a) spectral tracking
  doesn't do the control/residual split; (b) in *analog* decomposition tracking (the variant this
  thesis builds on) the residual is handled by analog tracking; weighted is the §4.2 extension.
  Attribute the split to decomposition tracking alone, "(analog or weighted)".
- **[MINOR]** `:27-28` — "colour is optimised per view": 3DGS colour is per-Gaussian SH optimised
  jointly over all views; the correct claim is that it bakes outgoing radiance under fixed
  illumination.
- **[MINOR]** `:28` — "EWA" never expanded or cited thesis-wide; expand + cite Zwicker et al., or
  drop the acronym.
- **[NIT]** `:55` — "Newton or bisection root-find" unconditionally: DSYG inverts single-Gaussian
  segments in closed form, reserving root-finding for multi-overlap segments; "root-finding within
  multi-primitive segments" is more precise.
- **Verified, no action:** Theorem-1 min-of-free-flights statement matches SDTracking §4.1; the
  gap→contribution chain (§3.2→§3.5) is sound and consistent with the corrected Ch 4; Mitsuba 3 /
  Dr.Jit, OpenVDB/NanoVDB, Woodcock, NeRF, StochasticSplats descriptions check out against the PDFs;
  all 13 cited keys exist; `images/flicker.png` resolves.

---

## What an examiner can rely on (verified-solid inventory)

- Build: `latexmk` clean, zero errors, all cross-references and citation keys resolve.
- Ch 2 mathematics: all core equations verified correct (the findings above are precision-of-prose,
  not wrong equations).
- Ch 4 code fidelity: the architecture as described — single any-hit trace, entries-only collection
  with analytic exits, SoA hit buffer, bit-vector/compact-set active set, closed-form free-flight
  inversion, segment-restricted inverse, megakernel, Welford, RR, sphere-GAS/IAS, caps and overflow
  counter, cap-estimator numbers — matches the implementation line-for-line, with the four
  exceptions listed in §4.
- Ch 5 record fidelity: furnace (+6.5% Mitsuba NEE), collinear stress test, 652-prim cloud, ladder
  structure, bias/variance methodology — all exactly per FINDINGS, with the attribution fixes in §5.
- Ch 8: limitations and future work are honest (no dead end resold as future work); contributions
  recap matches the chapters.
- Headline numbers: ~5× initial deficit (§8.5: 5.5×), overtake on the showcase (§8.16/8.17),
  firefly-free (§8.15), RIS 1.4×/K=6 (§8.37) — all real, subject to the scoping qualifiers above.
