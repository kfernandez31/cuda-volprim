# Thesis review — Jorge Condor (P1), round 2

*Co-advisor; lead author of* Don't Splat Your Gaussians *(DSYG); author of the Mitsuba* `volprim`
*reference this thesis races against, and of the argmin/analog-decomposition suggestion it builds on.*

Re-reviewed: full thesis (`thesis/latex/`, abstract + Ch 1–8), against the DSYG paper
(`papers/DSYG.pdf`), the `volprim` source (`~/jorge/volumetric_primitives`), the renderer source
(`device/`, `include/`, `src/`), and the banked campaign data (`results/campaign/`). This is the
round-2 pass: a twofold job — *verify* that the round-1 fixes are correct and complete, and bring
*fresh eyes* to anything new. Clock-independent numbers were re-derived from the banked seed EXRs and
CSVs; no local frame times were trusted (the GPU is power-capped).

---

## Honest bottom line

**The one Blocking gap from round 1 is closed, and closed correctly.** In round 1 I graded the thesis
Blocking on a single point: the headline 59× rests on the claim that *my* next-event estimator is
+156 % energy-biased on this cloud, and the thesis stated the *fact* of that bias without the
*mechanism* — leaving the most dangerous question in the defence unanswerable from the text. Round 2
supplies all three things that gap demanded: a stated, technically coherent mechanism (§7.2,
07-results.tex:116–123), a configuration record (07-results.tex:111–114), and a *measured* furnace
magnitude-bridge (07-results.tex:124–133, banked `results/campaign/furnace.md`). I checked the
mechanism line-by-line against my own `volprim` source — it is not merely plausible, it is *correct*:
`volprim`'s NEE evaluates the shadow-ray transmittance analytically and deterministically
(`volprim_prb.py` `eval_transmittance`, closed-form erf), then MIS-combines it on bare directional
densities against an analog free-flight continuation, so the two strategies do not share a measure;
stock `prbvolpath` ratio-tracks its shadow transmittance and passes the furnace. The furnace
single-Gaussian thickness sweep (+0.86 %→+4.51 %→+9.74 %→+30.94 % at σ-scale 2/4/6/12) reproduces
*exactly* from the banked EXRs. Every other load-bearing number I re-derived also reproduced: +156 %
(0.8199 vs 0.3201 analog GT, +156.14 %), the 0.4 % MIS agreement (0.3214), the bunny parity (0.99840),
the scaling exponent (0.397, R²=0.96), the 4090 ladder arithmetic, the 578/838 MiB memory. The
de-hyping I praised in round 1 is intact and, if anything, sharpened (the new CI is honestly footnoted
as variance-dominated; the memory comparison is honestly reported as *mixed*).

The round-1 domain Should-fixes are likewise all fixed and verified: DSYG is now correctly
characterised as inverting single-Gaussian segments in closed form and bisecting only in overlaps (S1,
six locations); the Condor credit is scoped to the sampler half everywhere (S2, all four locations,
note the deliberate "the latter" at 01-introduction.tex:37); the kernel-generality scope correctly
distinguishes the forward optical depth — closed-form for both kernels — from the practical inverse —
Gaussian-specific (S3, §2.5 and §8.2); the heavy-overlap residual now carries a *decided* finding —
double-precision erf⁻¹ shifts it by ~2×10⁻⁸, ruling out single-precision inversion (S4,
`results/campaign/s4_erfinv.md`); and `tab:overlap`↔`tab:vram` are reconciled with a stated convention
(B2, bunny 245→71→80, 387→464→528).

**There is no Blocking finding in this round.** That is the headline of the round-2 verdict.

**Where it is still weakest — and these are Should-fixes that *strengthen* the central argument, not
rescue it.** Reading §7.2 as the person who wrote the estimator being indicted, I am left with one
sharp follow-up the text does not pre-empt: *the mechanism explains why my NEE is biased, but my own
renderer also evaluates its shadow-ray transmittance analytically and deterministically
(04-architecture.tex:315) and does not ratio-track — so by the thesis's own stated mechanism, why is
**it** not biased too?* The answer exists in the work (this renderer MIS-combines two *light-sampling*
strategies over a shared analytic-transmittance measure and *suppresses* the continuation ray's direct
term, 04-architecture.tex:316–317 — it never MIS-combines NEE against the analog continuation, which is
the specific clash that bites `volprim`), but §7.2 never connects that contrast, and the "ratio
tracking would remove it" sentence describes how *stock prbvolpath* escapes the bias, not how *this
renderer* does. That is now the most dangerous question in the defence, and it is one sentence away from
closed. Two smaller residues: the furnace bridge *measures* the single-Gaussian thickness sweep to
~31 % but the leap to +156 % via "overlap and in-scatter recursion" is *argued, not separately swept*;
and the new CI [54, 63] does not state the clip/resample convention that yields that exact interval.

---

## Deliverable 1 — Grades

Dimensions I own in depth (1, 2, 3, 4, 6) are justified at length; the rest are graded briefly and
deferred to the relevant examiner. Round-1 grade in parentheses where it moved.

| # | Dimension | Grade | One-line justification |
|---|-----------|:-----:|------------------------|
| 1 | Technical correctness & soundness | **5** (was 4) | Math verified exact line-by-line, validated to ~10⁻⁴; the round-1 debits (DSYG mis-stated; residual cause open) are resolved (S1 corrected; erf⁻¹ ruled out). Sole residue is a localised Ch4↔Ch5 wording inconsistency, not a correctness defect. |
| 2 | Experimental rigor & methodology (fairness) | **4** (was 3) | B1 closed: the mechanism is now stated, coherent, and confirmed against my source; the furnace bridge is measured and reproduces. →5 once the mechanism's contrast with *this* renderer is drawn and the overlap-driven leap to 156 % is bounded rather than asserted. |
| 3 | Honesty & claim calibration | **5** (held) | The de-hyped register is intact: 59× scoped to peaky illumination and clipped variance, the bare sampler reported a net *loss* on flat lighting, memory reported *mixed*, the CI honestly footnoted variance-dominated. |
| 4 | Relevance & scope discipline | **4** (held) | Squarely relevant (accelerating a SIGGRAPH-2025 method) and disciplined; the round-1 Epanechnikov over-generalisation (S3) is precisely fixed. Held at 4 only because the Gaussian-only scope is inherent and the Gabor/Epanechnikov frontier is treated briefly. |
| 5 | Argumentation, narrative & significance | 4 | *(Didyk's.)* The central argument is now mechanistically complete; the one gap is the ours-immunity contrast (SF1). |
| 6 | Related work & positioning | **4** (held) | DSYG solver characterisation now precise everywhere; 3DGRT/EVER/StochasticSplats/OpenVDB positioning solid. Held at 4 for the unpinned 320-triangle citation (P-a). |
| 7 | Writing & academic style | 4 | *(Style editor's.)* Formal register, British spelling, strong topic sentences; some very dense multi-clause sentences. |
| 8 | Professionalism & presentation | 4 | *(Style editor's.)* Figures self-contained, captions load-bearing, the round-1 internal-doc leaks removed. |
| 9 | Cross-thesis consistency | **4** (held) | Round-1 inconsistencies (credit, DSYG solver) fixed; one *new* one introduced by the S4 fix (Ch4:245 still floats finite-precision; Ch5/§8.2 rule it out — SF3). |
| 10 | Defense-readiness | **4** (was 3) | *(Didyk's.)* The single most dangerous question (NEE mechanism) is now answerable from the text; only its follow-up (SF1) remains. |

**Owned-dimension detail.**

**(1) Correctness — 5 (was 4).** I re-verified §4.3 against my Eq. 13–16 and the code: the whitening
`y = S⁻¹Rᵀ(x−μ) = a + sb`, the completed square, the erf optical depth, and the inverse
`t(τ) = s* + (√2/w)·erf⁻¹(erf(u₀)+τ/C_k')` — all correct, all matching. The argmin exactness proof
(04-architecture.tex:231–243) is correct and the code draws genuinely independent uniforms per candidate
with a span-restricted inverse. Both round-1 debits are gone: DSYG is no longer mis-stated as always
root-finding (S1), and the heavy-overlap residual is now a *characterised* finding rather than an open
question — the double-precision erf⁻¹ test (`s4_erfinv.md`: overlap-centre shift 2.4×10⁻⁸ vs the
+2×10⁻⁴ residual, four orders of magnitude smaller) decisively rules out single-precision inversion.
I withhold nothing on the renderer's correctness; the only debit at this grade is expository (SF3).

**(2) Rigor & fairness — 4 (was 3).** This is the dimension that moves, and it moves because the
linchpin claim is now defended. The mechanism (07-results.tex:116–123) is exactly the diagnosis I would
give: an *analytic, deterministic* NEE transmittance MIS-combined on bare directional densities against
a *stochastic analog* continuation, so the measures do not match and a positive surplus survives at
interior vertices — curable by ratio-tracking the shadow ray. I confirmed every clause in my own source.
The config record (Gaussian kernel, bisection solver, max_depth 128 — conservative, can only *darken*)
forecloses the "you misconfigured my code" retreat, and the furnace establishes intrinsic-ness
reference-free and depth-invariantly. What keeps this from a 5: (a) the mechanism is stated only for
*why `volprim` fails*, and inadvertently implicates this renderer too, which also uses analytic
deterministic shadow transmittance (SF1); (b) the magnitude bridge *measures* a single-Gaussian sweep
to ~31 % but *argues* the rest of the way to +156 % via overlap and recursion (SF2); (c) the CI
convention is unstated (SF4). All three are presentational, not substantive — the claim is correct — but
a fairness dimension owned by the author of the code under test should leave none of them for the
viva floor.

**(3) Honesty — 5 (held).** I went looking again for overclaim creep in the round-2 additions and found
the same discipline. The new CI is footnoted as *variance-dominated* and the clip convention named as
"the most conservative of the firefly-discounting conventions" (07-results.tex:30–35); the memory result
is reported as *mixed*, with the tornado and bunny premiums stated plainly (07-results.tex:179–182); the
furnace bridge is presented as the *same defect in the thick, overlapping limit* rather than a clean
measured curve. The one place I would *add* a hedge — "argued, not separately swept" on the overlap leap
(SF2) — is the honest cousin of the rigor debit, not a violation. This remains a model of the calibrated
register the field needs.

**(4) Relevance — 4 (held).** The work accelerates the method I published last year and positions
cleanly against the live frontier. The round-1 debit — the optimistic "applies to either kernel"
framing — is now exactly right: §2.5 (02-background.tex:196–199) and §8.2 (08-conclusion.tex:47–53)
distinguish the *forward* optical depth (closed-form for the Epanechnikov too, my Eq. 19) from the
*practical inverse* the argmin needs (Gaussian-specific; my paper resorts to Newton–Raphson even for a
single Epanechnikov kernel), and explicitly flag extending the argmin to compactly-supported kernels as
open. That is the correct scoping. Held at 4 only because the frontier the representation is heading
toward — Epanechnikov, Gabor — is named but not developed; for a thesis whose future work leans on my
kernel programme, a paragraph on *what* the argmin would need there would lift this to a 5.

**(6) Related work — 4 (held).** The round-1 imprecision — characterising my solver as unconditional
root-finding — is fixed in all six locations, and the 3DGRT/EVER positioning is accurate and correctly
distinguished from DSYG's scattering transport (03-related-work.tex:53–63). Held at 4 for one citation
nit: the "320-triangle shell as best" figure (06-optimization.tex:240–241) is not in the DSYG main paper
body (the 4.96× is; the per-shell optimum is in the supplemental) — cite the supplemental or soften
(P-a).

---

## Round-1 fix verification (the round-2 mandate)

| Item | Round-1 severity | Status | Evidence |
|------|:----------------:|:------:|----------|
| **B1** — NEE-bias mechanism missing | Blocking | **CLOSED** | Mechanism stated (07-results.tex:116–123), confirmed against `volprim_prb.py` source; config recorded (111–114); furnace bridge measured + reproduces from `furnace.md` EXRs |
| **S1** — DSYG mis-stated as always root-finding | Should-fix | **FIXED** | Corrected at abstract:7–8, intro:17–19, §3.2:46–49, §4.1:9, tab:complexity:210/217, §4.4 — all say closed-form single-segment / bisection-in-overlap; matches DSYG §5.1 |
| **S2** — credit over-attribution | Should-fix | **FIXED** | All four scoped to the sampler half (abstract:16, intro:37 "the latter", §4.4:293, conclusion:14) |
| **S3** — kernel generality over-generalised | Should-fix | **FIXED** | §2.5:196–199 and §8.2:47–53 distinguish forward-depth (both) from inverse (Gaussian-only); matches DSYG §5.2 |
| **S4** — residual cause undecided | Should-fix | **FIXED** | erf⁻¹ ruled out (Ch5:223–227, §8.2:67–70, `s4_erfinv.md`: 2.4×10⁻⁸ shift) |
| **S5** — load-bearing figures unbanked | Should-fix | **MOSTLY FIXED** | Furnace now banked (`furnace.md` + EXRs); 16-spp denoiser figure (7.2×, 0.353→0.049) still reproduces only by recompute, not first-class in `g2_denoiser.md` (P-c) |
| **B2** — tab:overlap↔tab:vram (Talbot's) | Blocking/SF | **FIXED** | Convention stated (tab:overlap caption:547–553); bunny 245→71→80, 387→464→528 consistent |
| **P1** — back-face-cull flag scope | Polish | **FIXED** | 04-architecture.tex:87–88 now "only for the tessellated-icosphere variant" |

---

## Deliverable 2 — Findings (round 2)

Format: severity — `file:line` — issue — fix — quote. No Blocking findings this round.

### Should-fix

**SF1 — The §7.2 mechanism explains why *my* estimator is biased but not why *this renderer* is immune;
the stated cure describes `prbvolpath`, not this renderer.**
`07-results.tex:116–123`, with `04-architecture.tex:313–317`.
*Issue.* The mechanism pins `volprim`'s bias on an *analytic, deterministic* NEE transmittance
MIS-combined against a *stochastic analog* continuation, and names the cure as "estimating the
next-event transmittance with ratio tracking." But this renderer *also* evaluates its shadow-ray
transmittance analytically and deterministically (04-architecture.tex:315: "transmittance from the
analytic optical depth … along each shadow ray") and does *not* ratio-track. By the thesis's own stated
mechanism, the reader is entitled to conclude this renderer should be biased too — yet it is unbiased to
0.4 %. The actual reason it escapes is real and already in the work, but unstated at the point of the
mechanism: this renderer MIS-combines *two light-sampling strategies* (environment-by-luminance and
phase) over a *shared* analytic-transmittance measure, and *suppresses* the continuation ray's direct
environment term (04-architecture.tex:316–317) — so it never MIS-combines NEE against the analog
continuation, which is the specific measure clash that bites `volprim`. The "ratio tracking would
remove it" sentence is true of stock `prbvolpath` (verified in source) but is not how *this* renderer
avoids the bias, and a careful reader will misread it as implying this renderer ratio-tracks.
*Why Should-fix, not Blocking.* The claim is correct and empirically proven (furnace pass, 0.4 %
agreement); this is an expository completeness gap in the mechanism's *contrast*, not a defect in the
result. But it is the single follow-up question I would ask after the now-answered B1, so it is the
highest-value fix in the thesis.
*Fix.* Add one or two sentences after 07-results.tex:123 contrasting the two designs: this renderer's
MIS partitions the *direct-lighting* contribution between two light-sampling strategies that share the
analytic-transmittance measure (so the partition is exact), and routes the medium attenuation of the
continuation outside the NEE MIS by suppressing its direct term — whereas `volprim` MIS-combines the
analytic NEE term against the analog continuation across mismatched measures. State explicitly that
ratio-tracking is `prbvolpath`'s cure, not this renderer's.
*Quote.* "estimating the next-event transmittance with ratio tracking, as a standard volumetric path
tracer does, would remove it." (07-results.tex:122–123.)

**SF2 — The furnace bridge measures the single-Gaussian thickness sweep but *argues* the leap to +156 %.**
`07-results.tex:124–133`, `results/campaign/furnace.md`.
*Issue.* The bridge is grounded where it is measured: the single-Gaussian centre over-count grows
+0.86 %→+4.51 %→+9.74 %→+30.94 % at σ-scale 2/4/6/12, and this reproduces exactly from the banked EXRs.
But the banked curve tops out at ~31 % on a *single* Gaussian; the remaining ~5× to +156 % is attributed
to "heavy overlap … and the in-scatter recursion compounds it" — a verbal argument, not a second
measured sweep (e.g. an overlap-count sweep at fixed thickness). The argument is coherent and the
endpoint (+156 %) is independently measured on the cloud, but the *attribution* of the gap between 31 %
and 156 % to overlap-plus-recursion is asserted.
*Fix.* Either add one measured column (over-count vs overlap count at fixed σ-scale, if cheap) or, at
minimum, state in one clause that the overlap/recursion contribution is *argued from the single-Gaussian
trend and the measured cloud endpoint, not separately swept* — the same honest hedge the thesis applies
elsewhere.
*Quote.* "this is compounded by heavy *overlap* … and the in-scatter recursion compounds it over
scattering order, reaching +156 %." (07-results.tex:130–133.)

**SF3 — Ch4 still floats "finite-precision realisation" as a residual cause that Ch5 and §8.2 now rule
out.**
`04-architecture.tex:244–246` vs `05-validation.tex:222–227` and `08-conclusion.tex:67–70`.
*Issue.* The S4 fix decided the residual is *not* a single-precision erf⁻¹ artefact, and Ch5 and the
limitations list say so. But the Ch4 parenthetical was not updated to match: it still presents the
residual as the place "where this independence assumption or its finite-precision realisation is
stressed," offering finite-precision as a live candidate. An examiner reading Ch4 before Ch5 sees a
candidate that Ch5 then eliminates — a self-introduced inconsistency from the round-2 fix. (This also
subsumes round-1 P3: the parenthetical reads as a hedge.)
*Fix.* Reconcile Ch4:244–246 with the decided conclusion — drop "or its finite-precision realisation,"
or replace it with the two overlap-regime candidates Ch5 actually carries (the independence assumption
under overlap, or NEE shadow-ray transmittance from inside overlapping primitives), and state that
finite precision is *ruled out* (forward-referencing Ch5).
*Quote.* "Ch5 reports a small convergence-stable residual at heavy overlap, where this independence
assumption or its finite-precision realisation is stressed" (04-architecture.tex:244–246).

**SF4 — The headline CI [54, 63] does not state the convention that produces it.**
`07-results.tex:25–35`.
*Issue.* The point estimate (58.6× clipped k-ratio) reproduces to the digit, and the CI is plausible,
but I could not reproduce [54, 63] exactly under either canonical convention: a one-time global
99.9th-percentile clip gives a tighter [56.9, 60.5]; re-clipping inside each bootstrap resample gives a
looser [51.3, 67.4]. [54, 63] sits between them, so it is internally consistent but convention-sensitive
and not exactly reproducible from the text.
*Fix.* State, in the footnote, whether the 99.9th-percentile clip is computed once globally or
recomputed per resample, so the interval is reproducible. (Trivial, and it pre-empts a Talbot question
too.)
*Quote.* "roughly 59× (95 % bootstrap CI [54, 63] over the 16 seeds)" (07-results.tex:25–26).

### Polish

**P-a — 320-triangle icosphere figure not in the DSYG main paper.** `06-optimization.tex:240–241`. The
4.96× is in the body; the 320-triangle per-shell optimum is supplemental. Cite the supplemental or soften
to "an icosphere shell" + the reported 4.96×.

**P-b — The shipped `volprim` plugin routes single-Gaussian segments through the iterative solver.** In
the released code the closed-form single-segment fast path is gated off (`volprim_prb.py`, the
`if False:` single-primitive branch), so the *shipped* plugin this thesis benchmarks against actually
bisects even single-Gaussian segments. The thesis correctly describes the *paper's* algorithm (Eq. 16
for single kernels), which is faithful and, if anything, *generous* to the reference in the complexity
comparison. Optionally anchor the single-segment-closed-form claim to "as described in DSYG §5.1" to be
unimpeachable. Not an error.

**P-c — 16-spp denoiser figure not first-class in the ledger.** `g2_denoiser.md` documents a 64-spp run;
the figure's 7.2× / 0.353→0.049 reproduce only by recomputing from `denoise/*.exr`. Bank the 16-spp
inputs with a one-line provenance note (round-1 S5's remaining half).

---

## Deliverable 3 — Structural recommendations (round 2)

1. **The bias story is now correctly consolidated — finish the cross-reference.** Round-1 rec 1 (give
the bias a dedicated furnace→mechanism→magnitude arc) is effectively done: the furnace invariant lives
in §5.5, the mechanism + magnitude in §7.2, and §7.1 cites §7.2 for the bias. The only thing left to
make it bullet-proof at the viva is to fold SF1 into §7.2 so the arc reads furnace → *why volprim fails
and why this renderer does not* → magnitude. That single addition converts the strongest objection below
from a live trap into a demonstration of rigour.

2. **Withdraw the round-1 suggestion to trim `tab:overlap`.** In round 1 I suggested cutting two density-
fit rows. Now that the table's caption is load-bearing for the B2 reconciliation (estimator vs measured
vs shipped) and the dense/fine fits carry the "overlap ≠ primitive count" point that the scaling
narrative (§7.7) depends on, I would *keep* all seven rows. The point is now doing real work.

3. **`tab:ser-eq` (the 4090 ladder) relocation remains optional.** I still find §6.9 ending an
optimisation chapter with an end-to-end results ladder slightly odd, and it would sit naturally beside
the 59× in §7.1 badged as the Ada corroboration — but this is explicitly an author-deferred call and the
current placement is defensible. Not pressing.

4. **No cuts to the negative-results ledger (§6.8).** Unchanged from round 1: it is a genuine
contribution and should stay at full length.

---

## Deliverable 4 — Strongest objection (round 2)

*The single question I would now open the defence with* — note that round 1's opener (the bare NEE
mechanism) is now answered in the text, so the dangerous question has moved one step downstream:

**"Your §7.2 mechanism says my next-event estimator is biased because it MIS-combines an *analytic,
deterministic* shadow-ray transmittance against a *stochastic analog* continuation across mismatched
measures, and that ratio-tracking the shadow ray would cure it. But your own renderer evaluates its
shadow-ray transmittance analytically and deterministically too — §4.4 says so explicitly — and it does
not ratio-track. By the very mechanism you have just stated, why is *your* estimator not biased? What,
precisely, is different about how *you* combine next-event estimation with the rest of the path?"**

This is the most dangerous question because it turns the thesis's own newly-stated mechanism against it:
if the author cannot articulate why this renderer escapes the defect it diagnoses in mine, the +156 %
diagnosis looks selectively applied, and an examiner could wonder whether the 0.4 % agreement is luck
rather than design. The thesis currently states the mechanism (correctly) and the cure (ratio tracking,
which is `prbvolpath`'s route, not this renderer's) but never closes the loop on its own immunity.

**How the author should pre-empt it — and the good news, again, is that the answer is correct and
already latent in the work (04-architecture.tex:313–317):**

1. *State the structural difference in one sentence at the mechanism.* This renderer's MIS does not pit
NEE against the analog continuation at all. It partitions the *direct-lighting* contribution at a vertex
between two *light-sampling* strategies — environment-by-luminance and phase-function sampling — both of
which carry the *same* analytic-transmittance measure, so the balance-heuristic partition is exact. The
continuation ray's direct environment term is *suppressed* (04-architecture.tex:316–317) precisely so it
is never double-counted against next-event estimation. There is therefore no measure mismatch to leak a
surplus.

2. *Name what `volprim` does differently.* `volprim` MIS-combines its analytic NEE term against the
*analog continuation's* survival on bare directional densities — two estimators of the medium term under
two different measures. That is the clash. Stock `prbvolpath` removes it by ratio-tracking the shadow
ray (drawing the transmittance from the *same* measure as the continuation); this renderer removes it by
not MIS-combining across measures in the first place. Two different cures for the same defect — and this
renderer's is the one that matters for the comparison.

3. *Anchor it empirically.* The furnace pass (energy-conserving, reference-free, depth-invariant) and
the 0.4 % converged agreement are the proof that this renderer's partition is exact; the +156 % is the
proof that `volprim`'s is not. The mechanism explains *both* signs once the contrast in (1)–(2) is in
the text.

If those three moves are added — perhaps three sentences appended to §7.2 — this question goes from a
viva-floor trap to a clean demonstration that the author understands the estimator boundary better than
the boundary itself was documented. The claim is right. Make the *contrast* explicit, not just the
defect.

---

*Overall (Condor's view): a publishable-quality systems-and-validation contribution that accelerates my
method honestly. The one Blocking gap from round 1 — the NEE-bias mechanism — is now closed and verified
correct against my own source; all four domain Should-fixes are fixed; every load-bearing number
reproduces. **Verdict: minor revisions.** The remaining work is three Should-fixes that strengthen the
central argument rather than rescue it — chiefly SF1, the one sentence that explains why this renderer is
immune to the very bias it diagnoses in mine. Close that, and the most dangerous question in the defence
becomes the thesis's strongest moment.*
