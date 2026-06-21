# Thesis review — Jorge Condor (P1)

*Co-advisor; lead author of* Don't Splat Your Gaussians *(DSYG); author of the Mitsuba* `volprim` *reference this thesis races against, and of the argmin/analog-decomposition suggestion it builds on.*

Reviewed: full thesis (`thesis/latex/`, abstract + Ch 1–8), against the DSYG paper (`papers/DSYG.pdf`), the `volprim` source (`~/jorge/volumetric_primitives`), the renderer source (`device/`, `include/`, `src/`), and the banked campaign data (`results/campaign/`). Clock-independent numbers were re-derived from the banked seed EXRs and CSVs; no local frame times were trusted (the GPU is power-capped).

---

## Honest bottom line

This is a strong thesis, and an honest one — unusually so. The architecture is faithfully described and faithfully implemented (I checked the whitening derivation, the erf optical depth, the argmin exactness proof, and the inverse against both my paper and the student's code; all correct). Every load-bearing number I could re-derive reproduced exactly from the banked EXRs — the +156 % NEE bias, the 59×, the 0.9984 bunny parity, the memory table, the scaling exponent, the 4090 ladder. The de-hyping the author prizes is real and consistently applied: the 59× is scoped to peaky illumination and clipped variance, the bare sampler is reported as a *net loss* at equal quality on flat lighting, the analytic sphere is reported as *slower* than the tessellated one, the memory comparison is reported as *mixed*, and the negative-results ledger is a genuine contribution rather than a confession.

**Where it is systemically strong:** honesty and claim calibration (dimension 3), and the validation/comparison *methodology* — interleaved A/B, multi-seed bias/variance separation, an independent voxel-grid cross-check, the furnace invariant, equal-quality `k·t`. **Where it is weakest, and it is the one thing standing between this draft and a clean defence:** the load-bearing claim that *my* NEE estimator is +156 % energy-biased — the claim the entire 59× headline rests on — is asserted and evidenced but never *mechanistically explained*. I have satisfied myself that the bias is intrinsic and not a misconfiguration of my code (see below), but the thesis as written cannot demonstrate that to a sceptic, and I am the sceptic it must convince. That is the single most dangerous gap, and I have graded it Blocking — not because the claim is wrong (it is correct), but because it is currently undefended.

Two further findings are squarely in my domain and the student should want them addressed because they *help*: my own reference (DSYG) already uses the closed-form inverse for single-Gaussian segments and only bisects in overlaps — so the argmin's true novelty is extending closed-form sampling *into* the overlap regime, which the thesis under-sells; and the argmin's root-find-free advantage is Gaussian-*specific*, because the Epanechnikov inverse is impractical (my paper resorts to Newton–Raphson for it), which the thesis over-generalises.

---

## Deliverable 1 — Grades

Dimensions I own in depth (1, 2, 3, 4, 6) are justified at length; the rest are graded briefly and deferred to the relevant examiner.

| # | Dimension | Grade | One-line justification |
|---|-----------|:-----:|------------------------|
| 1 | Technical correctness & soundness | **4** | Renderer correctness is excellent and thoroughly validated to ~10⁻⁴; the only debits are expository (DSYG mis-stated as always root-finding; the heavy-overlap residual mechanism left as two candidates). |
| 2 | Experimental rigor & methodology (fairness) | **3** | Methodology is otherwise 4–5 (interleaved, multi-seed, independent cross-check, furnace), but the headline's foundation — *why* my NEE is +156 % biased — has no stated mechanism and the furnace EXR is unbanked; high-3, →4 the moment the mechanism lands. |
| 3 | Honesty & claim calibration | **5** | A model of the de-hyped stance: every headline is scoped, the bare sampler's flat-env *loss* is reported, the analytic sphere is admitted *slower*, the negatives are sold as contribution. |
| 4 | Relevance & scope discipline | **4** | Squarely relevant (accelerating a SIGGRAPH-2025 method) and disciplined in scope; the one debit is the optimistic "applies to either kernel" framing of the Epanechnikov/Gabor frontier. |
| 5 | Argumentation, narrative & significance | 4 | *(Didyk's.)* Coherent narrative, significance argued not asserted, negatives integrated; the NEE-mechanism gap weakens the central argument. |
| 6 | Related work & positioning | **4** | Well-positioned (3DGS, 3DGRT, EVER, StochasticSplats, decomposition tracking, OpenVDB/NanoVDB); the DSYG solver-fallback detail (§5.1) is the one place the prior art is imprecisely characterised. |
| 7 | Writing & academic style | 4 | *(Style editor's.)* Excellent formal register, British spelling, strong topic sentences; occasional very dense multi-clause sentences. |
| 8 | Professionalism & presentation | 4 | *(Style editor's.)* Figures self-contained, captions load-bearing, notation disciplined. |
| 9 | Cross-thesis consistency | 4 | Two localised inconsistencies: credit scope (intro/conclusion vs abstract/§4.4) and the "reference root-finds" characterisation (§3.2 vs abstract/§4.1/tab:complexity). |
| 10 | Defense-readiness | 3 | *(Didyk's.)* Largely ready, but the single most dangerous question (NEE mechanism) is currently unanswerable from the text. |

**Owned-dimension detail.**

**(1) Correctness — 4.** I verified §4.3 line by line: the whitening `y = S⁻¹Rᵀ(x−μ) = a + sb`, the completed square `½‖y‖² = ½w²(s−s*)² + ½d⊥²` with `s* = −(a·b)/w²` and `d⊥² = ‖a‖² − (a·b)²/w²`, the optical depth `τ = C_k·e^{−½d⊥²}·√(π/2)·(1/w)·[erf(u₁)−erf(u₀)]`, and the inverse `t(τ) = s* + (√2/w)·erf⁻¹(erf(u₀)+τ/C_k')` — all algebraically correct, all matching my own Eq. 13–16, and all matching the code (`primitive.h`, `sampling.cuh`). The argmin exactness proof (`Pr[min t_k > t] = ∏ e^{−τ_k(t)} = e^{−τ(t)} = T(t)`, 04-architecture.tex:229-244) is correct, and the code draws genuinely independent uniforms per candidate over both the active set and the hit buffer, with a span-restricted inverse to avoid the rejection bias — exactly as alg:argmin claims. The renderer is validated to ~10⁻⁴ in the mean with a sensible, falsifiable methodology. I withhold the 5 only for the expository debits in dimension 6 and the unresolved residual mechanism (S4).

**(2) Rigor & fairness — 3.** This is the dimension I grade hardest and the one I care about most, because the 59× rests on a claim about *my* renderer. The good: the comparison is genuinely fair in construction — identical hardware, identical primitive geometry, matched integrator/σ-scale/camera/lighting, and the benchmark is against `volprim`'s *unbiased analog* mode, not against the biased NEE (07-results.tex:22-32, 06-optimization.tex:463-466). The student is not "beating a broken estimator"; the bias claim only justifies *why* the honest Mitsuba baseline must be the firefly-noisy analog. The methodology around it is excellent. The debit, and it is the whole reason this is a 3 and not a 4: the +156 % bias is the linchpin of the headline and it is never explained (B1). A fairness dimension cannot score "strong" when its single load-bearing claim is supported only by an unexplained number and a furnace test whose magnitude (+6.5 %) is ~24× smaller than the cloud number it is asked to vouch for, with no bridge drawn between them.

**(3) Honesty — 5.** I went looking for overclaim and found the opposite discipline everywhere. 07-results.tex:34-52 reports the bare argmin sampler as ~0.6× — a *net loss* — at equal quality on flat lighting, and states plainly "What actually wins at equal quality is the MIS direct-lighting estimator … not the argmin sampler on its own." The 59× is footnoted as clipped-variance-specific with the raw ratio (~2000×) disclosed (07-results.tex:26-32). The icosphere ℓ=2 throughput option is flagged ~0.1 % biased wherever it appears. The Ada ladder is repeatedly scoped as "corroborate, not replace" and "most of the factor … variance, not speed." This is precisely the calibrated, non-hyped register the field needs more of. The only honesty wrinkle is a *generous* one — over-crediting me (S2) — which is the right kind of error to make but still wants fixing.

**(4) Relevance — 4.** The work accelerates the method I published last year and positions cleanly against the live frontier (3DGS rasterisation, the concurrent ray-traced-Gaussian appearance methods 3DGRT/EVER, sort-free rasterisation). Scope discipline is exemplary — no emission, Gaussian-only, single-frame, all bounded explicitly. The debit is frontier precision: §2.5 and §8.2 imply the analytic machinery and the argmin extend to "either" kernel, but the argmin needs a *practical closed-form inverse*, which is Gaussian-specific (S3). For a thesis whose future-work section leans on my kernel programme, this is worth getting exactly right.

**(6) Related work — 4.** Coverage and positioning are good and the 3DGRT/EVER additions close the gap a previous review flagged. The one imprecision is the characterisation of *my* method's solver: the thesis repeatedly frames DSYG as reaching the scatter distance by root-finding, when DSYG uses the closed-form inverse for single-Gaussian segments and reverts to bisection only for multi-Gaussian overlaps (S1).

---

## Deliverable 2 — Findings

Format: severity — `file:line` — issue — fix — quote.

### Blocking

**B1 — The +156 % NEE-bias mechanism is missing; the 59× headline rests on it.**
`05-validation.tex:205-211` (furnace) and `07-results.tex:100-112` + `fig:g1-bias` (cloud).
*Issue.* The thesis's central performance claim is that the only unbiased configuration of my reference is its high-variance analog mode, *because* its NEE estimator is +156 % too bright on this medium. This is load-bearing for the 59× and for the entire framing of the comparison. The thesis states the *fact* of the bias and gives one furnace number, but never gives the *mechanism* — why a next-event estimator over-counts direct light by a factor of 2.56 on a dense scattering cloud — and never bridges the furnace's +6.5 % to the cloud's +156 % (same estimator, ~24× apart). The complete causal text is six words: *"an over-estimate from its direct-lighting term."*
*Why this is Blocking and not Should-fix.* I wrote `volprim`. The first question I ask at the defence is "you claim my NEE is +156 % biased — convince me that is intrinsic to the estimator and not a misconfiguration of my code." The current text cannot answer it. I have done the work myself and the claim *is* correct — the thesis runs `volprim_prb` at its own documented defaults (Gaussian kernel, bisection solver, `max_depth=128`, which is *conservative* and can only darken), and the bias is intrinsic: `volprim`'s NEE MIS-combines an *analytic, deterministic* shadow-ray transmittance against an *analog-survival* continuation term on their bare directional pdfs, so the two strategies do not share a sampling measure and the partition is not exact; stock `prbvolpath` instead uses ratio-tracking for its NEE shadow ray and passes the furnace. The furnace→cloud magnitude jump is the optically-thick limit: as the analog escape probability collapses under σₜ×7.5 with tens of overlaps, the continuation term vanishes while the analytic NEE term stays finite at every interior vertex and compounds over scattering order. But *none of this is in the thesis* — it is recoverable only by reading my source. The claim is true, verified, and currently indefensible from the text.
*Fix.* (i) Add two–three sentences naming the mechanism — analytic-deterministic NEE transmittance MIS-combined against the stochastic analog continuation, contrasted with the ratio-tracking that makes stock `prbvolpath` energy-conserving — and state the optical-thickness argument that bridges +6.5 % (furnace) to +156 % (cloud). (ii) Lean harder on the furnace as the *reference-free, setup-independent, depth-invariant* proof it already is (it passes my analog mode and fails my NEE mode with zero dependence on the cloud asset, the σ-scale, or the student's renderer). (iii) Bank the furnace EXR/log (currently it traces only to `FINDINGS.md §8.1`; the cloud +156 % is fully banked, the furnace +6.5 % is not).
*Quote.* "Notably, Mitsuba's *next-event* integrator fails the same test by ≈6.5 % (an over-estimate from its direct-lighting term)" (05-validation.tex:208-210); "the reference's NEE result is 0.8199, *+156 % too bright*" (07-results.tex:102).

### Should-fix

**S1 — DSYG is mis-stated as always root-finding; this under-sells the student's own delta.**
`abstract.tex:11-13`, `01-introduction.tex:17-18`, `04-architecture.tex:8`, `tab:complexity` (06-optimization.tex:203-223).
*Issue.* The thesis characterises my reference as reaching the scatter distance by marching and root-finding, full stop. In fact DSYG uses the closed-form inverse (my Eq. 16) for *single-Gaussian* segments and reverts to bisection *only* where more than one Gaussian overlaps. The thesis's §3.2 gets this right ("within each multi-primitive segment", 03-related-work.tex:49-50), but the abstract, §4.1, and tab:complexity drop the qualifier and imply the reference root-finds unconditionally. This both misrepresents DSYG and — more importantly for the student — hides the real novelty: the argmin extends *closed-form per-primitive sampling into the overlap regime*, exactly where my own renderer must fall back to bisection.
*Fix.* State that the reference already samples single-Gaussian segments in closed form but bisects in overlaps, and frame the argmin's contribution as eliminating that overlap-regime root-find. This is more accurate *and* a stronger claim.
*Quote (my paper).* "For segments where more than one 3D Gaussian is contributing, we revert to using the bisection solver … For single kernels, we normally use Equation 16." (DSYG §5.1, p.6.)

**S2 — Credit over-attribution: the introduction and conclusion credit *both* architecture halves to me.**
`01-introduction.tex:30-33` and `08-conclusion.tex:11-16`.
*Issue.* The abstract (abstract.tex:19-24) and §4.4 (04-architecture.tex:291-293) correctly attribute *only* the analog-decomposition/argmin sampler to my suggestion. But the intro's contributions bullet and the conclusion's summary attach "an approach suggested by Condor … in two complementary halves: single-trace any-hit collection … and analog-decomposition scatter sampling", which reads as crediting me with the single-trace collection too. I did not suggest the single-trace any-hit collection — that, and the entire CUDA/OptiX realisation, is the student's engineering, and I do not want credit for it. An examiner comparing the four statements will notice the inconsistency.
*Fix.* Scope the attribution to the sampler half in both places, e.g. "… in two complementary halves: single-trace any-hit collection with analytic exits, and analog-decomposition (argmin) scatter sampling — the latter realising an approach suggested by Condor …".
*Quote.* "a single-trace … rendering architecture … realising … an approach suggested by Condor … in two complementary halves" (01-introduction.tex:30-33).

**S3 — The argmin's advantage is Gaussian-specific; "applies to either kernel" over-generalises.**
`02-background.tex:192-196` and `08-conclusion.tex:47-51`.
*Issue.* §2.5 says the thesis retains the Gaussian "but the analytic machinery applies to either" kernel, and §8.2 says "each would need its own closed-form integral, which the architecture could accommodate." The *forward* optical depth is indeed closed-form for the Epanechnikov (my Eq. 19) — so §8.2's "closed-form optical depth is Gaussian-specific" is itself slightly off — but the argmin needs a practical closed-form *inverse*, and for the Epanechnikov that inverse is impractical: my paper resorts to Newton–Raphson even for a *single* Epanechnikov kernel. So the argmin's sort-free, root-find-free property does *not* transfer to my kernel; for Epanechnikov it would degrade to per-primitive numerical inversion. This matters precisely because the Epanechnikov (and the Gabor extension) is where the representation is heading.
*Fix.* Distinguish the forward optical depth (closed-form for both kernels) from the practical closed-form inverse (Gaussian-specific) that the argmin requires, and scope the "applies to either" claim to the former. Note explicitly that extending the argmin to compactly-supported kernels is open precisely because their inverse is not practically closed-form.
*Quote (my paper).* "While there is an analytic solution for inverting Equation 12 for one Epanechnikov kernel, it is not practical, given its complexity. Instead, we directly use the Newton–Raphson solver" (DSYG §5.2, p.6).

**S4 — The "exact, not an approximation" claim sits unresolved against the heavy-overlap residual.**
`04-architecture.tex:240-247`, `05-validation.tex:213-223`.
*Issue.* §4.4 states the argmin "is an exact sample from the mixture, not an approximation," then immediately caveats "exact for the 3σ-bounded kernel both renderers use; Ch5 reports a small convergence-stable residual at heavy overlap, where this independence assumption or its finite-precision realisation is stressed." Ch5 then lists two candidate causes (the argmin distribution under overlap, or NEE shadow-ray transmittance from inside overlapping primitives) without deciding. The mathematics is genuinely exact (I checked), so a +2×10⁻⁴ convergence-stable residual most likely comes from the GPU `erf⁻¹` precision that I myself flag in DSYG §5.1 ("[Kirk 2007] produces large errors for certain input values"). Leaving the cause open invites the question "then is your argmin actually exact?"
*Fix.* Run the cheap discriminating test — re-render the cluster rung with a higher-precision `erf⁻¹` (or float64) and report whether the residual shrinks. If it does, attribute it to `erf⁻¹` precision (and cite my own note); if it does not, the independence/containment hypothesis stands and should be stated as the conclusion. Either way, replace "its likely causes are X or Y" with a decided answer.
*Quote.* "the argmin is an exact sample from the mixture, not an approximation (exact for the 3σ-bounded kernel both renderers use; Ch5 reports a small convergence-stable residual at heavy overlap …)" (04-architecture.tex:240-243).

**S5 — Two load-bearing figures are not banked in the campaign `.md` ledger.**
*Issue.* The furnace +6.5 % (B1) traces only to `FINDINGS.md §8.1`, with no banked EXR/log; and the denoiser figure's headline numbers (16-spp, RMSE 0.353→0.049, 7.2×, `fig:denoise`) are not in `g2_denoiser.md` (which documents a separate 64-spp run) and reproduce only by recomputing from `denoise/*.exr`. Both reproduce *in principle*, but for a thesis whose credibility rests on "every number traces to `results/campaign/`," the furnace one in particular is load-bearing for the central bias claim and should be first-class.
*Fix.* Bank the furnace render (NEE and analog arms) and the 16-spp denoiser inputs with a one-line provenance note in the campaign ledger.

### Polish

**P1 — Back-face-cull flag scope.** `04-architecture.tex:86-87`. The flag `OPTIX_RAY_FLAG_CULL_BACK_FACING_TRIANGLES` is emitted *unconditionally* in the trace path (`trace.cuh`), where it is a no-op for the built-in sphere; the sentence implies it is present only in the icosphere variant. Reword: "is a no-op for the built-in sphere and does work only in the tessellated-icosphere variant."

**P2 — `O(H²)` is the as-implemented bound.** `tab:complexity`, 06-optimization.tex:210. The reference's boundary selection is `O(H²)` as written in my Listing 1 (running minimum), not fundamentally; a heap would be `O(H log H)`. A half-clause ("as implemented") pre-empts the nitpick without weakening the point — the argmin's *single unordered* pass is the real win regardless.

**P3 — Clarify why truncation preserves exactness.** `04-architecture.tex:242`. The parenthetical "exact for the 3σ-bounded kernel" could note in one clause that truncation does not break the identity *because* the same bound is applied to each `τ_k` and to the combined `τ`; as written it reads as a hedge rather than a statement that the bounded case is still exact.

---

## Deliverable 3 — Structural recommendations

1. **Consolidate the bias story into one place (do this first).** The thesis's single most-defended claim is currently split across §5.5 (furnace, the intrinsic evidence) and §7.2 (cloud, the magnitude), with the mechanism in neither. Give it a short dedicated subsection — furnace invariant → mechanism → cloud magnitude — cross-referenced from the 59× headline and from §6.9's ladder. This is structural, not cosmetic: it is what turns B1 from Blocking into closed.

2. **Trim `tab:overlap` (06-optimization.tex:531-554) to the load-bearing rows.** The seven-row table includes four density fits of the Disney cloud to make the "overlap ≠ primitive count" point. Two rows (primary + the dense 4096 fit) make it as forcefully; move the coarse and fine fits to an appendix. The point is excellent; the table is heavier than the point needs.

3. **Consider moving `tab:ser-eq` (the 4090 ladder) into Ch7 (Results).** §6.9 reads slightly oddly because it ends an *optimisation* chapter with an end-to-end *results* ladder. Keep the SER mechanism and `tab:ser` (the per-asset image-identical speedups) in Ch6, but the equal-quality ladder vs the reference (`tab:ser-eq`) is a results artefact and would sit more naturally beside the 59× in §7.1, clearly badged as the Ada corroboration. Optional; the current placement is defensible.

4. **No cuts to the negative-results ledger.** §6.8 is a genuine contribution and should stay at full length; resist any temptation to compress it for space.

---

## Deliverable 4 — Strongest objection

*The single question I would open the defence with:*

**"You stake your headline 59× on the claim that my next-event estimator is +156 % energy-biased on this cloud — that my own renderer's fast mode is broken, leaving only its noisy analog mode as a fair baseline. Show me the mechanism. Why does a next-event estimator over-count direct light by a factor of two and a half here, when your furnace test — the only place you quantify this bias directly — puts it at six and a half percent? What in `volprim` produces the other twenty-four-fold, and how do you know the +156 % is a property of my estimator rather than of how you invoked it?"**

This is the most dangerous question because it attacks the foundation, not the periphery: if the +156 % is a setup artefact, the 59× collapses to the ~0.6× the bare sampler honestly admits on flat lighting (07-results.tex:48), and the thesis's headline becomes a methodology error in front of the person who wrote the code being measured. The thesis currently meets it with one clause — "an over-estimate from its direct-lighting term" — which is an assertion, not an answer, and the +6.5 %→+156 % gap is left entirely unbridged.

**How the author should pre-empt it — and the good news is that the answer is correct and already latent in the work:**

1. *Lead with the furnace as a reference-free proof of intrinsic-ness.* In an albedo-1, zero-absorption furnace, energy conservation demands every pixel return the background for *any* correct estimator — no asset, no σ-scale, no calibration, no dependence on the student's renderer enters. My analog mode passes it; my NEE mode fails it by +6.5 %, and that failure is *depth-invariant* (identical at `max_depth` 32 and 256). That alone establishes the bias is a property of the NEE+MIS estimator, not of the setup. This is the author's strongest card and it is currently played face-down.

2. *Name the mechanism.* My `volprim` NEE adds an *analytic, deterministic* shadow-ray transmittance at every scatter vertex and MIS-combines it, on bare directional pdfs, against a continuation term that carries the medium attenuation *stochastically* via analog free-flight survival. The two strategies do not share a sampling measure, so the MIS partition is not exact and a direct-lighting surplus survives at interior vertices. Stock `prbvolpath` avoids exactly this by ratio-tracking its NEE shadow ray — and stock `prbvolpath` passes the furnace. (The author should verify this characterisation against my source and state it in their own words; the contrast with `prbvolpath` is the clinching evidence.)

3. *Bridge the magnitude.* The furnace is optically thin, so the analog continuation almost always escapes and the discrepancy is small (+6.5 %). The cloud at σₜ×7.5 with tens of overlaps is optically thick: the analog escape probability collapses, the continuation term shrinks toward zero, the analytic NEE surplus stays finite at every one of many interior vertices, and the in-scatter recursion compounds it over scattering order — arriving at +156 %. Same defect, two regimes.

4. *State that you benchmark against my unbiased mode, not the biased one.* The 59× is computed against `volprim_prb` in `use_nee=false` analog — the bias claim only justifies *why* that noisy mode is the honest baseline. You are not racing a broken estimator; you are racing my correct-but-noisy one. Say so at the point of the headline.

If those four moves are in the text — and they cost perhaps half a page plus banking one furnace render — this question goes from a thesis-ending trap to a demonstration of exactly the kind of rigour the rest of the thesis already shows. The claim is right. Make it defensible.

---

*Overall (Condor's view): a publishable-quality systems-and-validation contribution that accelerates my method honestly and characterises where the effort pays off. Minor-to-moderate revisions: close B1 (the mechanism), correct the DSYG solver characterisation and the kernel-generality scope (S1, S3), and fix the credit attribution (S2). None of these touch the results — they make the thesis say, accurately, what its own data already supports.*
