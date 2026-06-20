# Honesty / overclaiming sweep — whole thesis (2026-06-15)

Examiner pass over abstract + Ch1–8, calibrated against `results/campaign/*.md` and `thesis/FINDINGS.md`.
De-hyping is the author's stated priority; the brief is to hunt overclaims **and** flag under-claims, but
not to push hype. Severity: **Blocking** (unsupported / self-contradictory) / **Should-fix** (needs
hedge or scope) / **Polish** (filler superlative / minor).

Bottom line up front: the de-hyping is **substantially complete and largely consistent**. Every load-bearing
quantitative claim I checked reproduces from the recorded data (59×, 156%, 31% memory, 68× s.e. ratio,
N^0.40, 1.48× RIS, 7.2×/28-30× denoiser, wavefront 100–1400×). The abstract is carefully scoped. The
**one genuine regression** is the bare "novel rendering architecture" in the conclusion (Ch8:11), which the
abstract and Ch4 both correctly avoid. A second loose spot is the unscoped "substantially faster" goal in Ch1.

---

## BLOCKING

**None.** No claim is unsupported by the evidence or internally contradictory at a blocking level. (The
"novel" regression below is one word and arguably Should-fix, but I rank it at the top of Should-fix because
the prior review explicitly flagged it as CRITICAL/overclaiming and it survived in one location.)

---

## SHOULD-FIX

### S1 — Ch8 conclusion `08-conclusion.tex:11`: unqualified "novel" (REGRESSION of prior act-first #7)
> "The central contribution is a \emph{novel rendering architecture} (\Cref{ch:architecture}) with two
> complementary halves: ..."

**Why it overclaims.** Ch4's own "Novelty and prior art" paragraph (`04-architecture.tex:284-293`) is careful:
"The components of this scheme are individually known" (decomposition tracking = Kutz 2017; closed-form
Gaussian optical depth = DSYG; sort-free spirit = StochasticSplats); "The contribution is their
\emph{synthesis} ... suggested by Condor, a co-author of the reference ... a direction its authors
anticipated but had not implemented ... to the author's knowledge for the first time for this
representation." The abstract mirrors this correctly ("a direction the reference's authors anticipated but
did not implement"). The bare word "novel" in the conclusion drops all of that hedging and contradicts the
chapter it summarises. The prior review (`2026-06-10-full-review-ex-ch6.md`, act-first #7, §1) flagged exactly
this as an overclaim; it was fixed in the abstract but NOT in the conclusion.

**Tighten:** "The central contribution is a rendering architecture (\Cref{ch:architecture}) — a synthesis,
new for this representation, of analog decomposition tracking with the reference's closed-form Gaussian
optical depth (a direction Condor, a co-author of the reference, anticipated but did not implement) — with
two complementary halves: ..."

### S2 — Ch1 `01-introduction.tex:22`: "substantially faster than the reference" is unscoped
> "The goal, from the outset, was efficiency: to render Gaussian kernel-mixture volumes both correctly and
> substantially faster than the reference ..."

**Why it overclaims (mildly).** This is framed as a *goal* ("The goal ... was"), which softens it, but it
reads as the thesis's achieved headline and carries no scope. The evidence is regime-specific: the
production headline (~59×) is **environment-importance-sampling-specific** (`g1_flat.md`: "the meadow ~59x
is ENTIRELY environment-importance-sampling"); the **core sampler** at equal quality is NET ~0.6× — i.e.
*slower* — on flat lighting (`g1_flat.md`: ours-analog 2.85s + ~5× variance → "net equal-quality on flat is
~0.6x"). Ch7 itself is scrupulous about this ("That 59× is specific to peaky illumination, and honestly
so"; "not a generic sampler win"). The Ch1 sentence should not promise a generic speed win that Ch7 then
spends a paragraph un-promising. The abstract gets this right (it ties the 59× to "the reference's only
unbiased (analog) configuration" and peaky env). Ch1 should at least gesture at the scope.

**Tighten:** "... to render Gaussian kernel-mixture volumes correctly and, under the production
(environment-lit) operating point, substantially faster than the reference, and to chart where the
performance ... can and cannot be improved." (Or keep it as a goal but add: "— a goal the later chapters
qualify to peaky-environment lighting.")

### S3 — Ch4 `04-architecture.tex:7`: "the sorted sequence of primitive boundaries" (residual-sorting, soft)
> "The reference~\cite{DSYG} marches the ray segment by segment through the \emph{sorted sequence} of
> primitive boundaries and root-finds the scatter distance ..."

**Why it risks overclaiming.** The prior review (act-first #2) established — verified against
`volprim/integrators/common.py` and echoed in `FINDINGS.md:678,701-703` — that the reference does **not**
perform a sort; it selects the next boundary by a *running minimum* (the explicit sort machinery,
`device/core/sorting.cuh`, lived only in *this renderer's own* former shadow path and was deleted). The
related-work chapter was corrected to say exactly that ("selecting the next boundary by a running minimum",
`03-related-work.tex:55-56`). "Marches ... through the sorted sequence" is *technically* defensible (a
running-min does visit boundaries in sorted order without sorting), but it reads as attributing a sort to
the reference and is in mild tension with the corrected related-work text. Note the same sentence's "with no
sorting, no marching, no root-finding" (the renderer's own property) is fine.

**Tighten:** "... marches the ray segment by segment from one primitive boundary to the next (selecting each
by a running minimum) and root-finds the scatter distance ..." — matching `03-related-work.tex:55`.

*(Not flagged, verified OK: the Ch6 complexity table row "Boundary sort (scatter) → eliminated"
(`06-optimization.tex:202`) is honest — `sec:complexity` states its "Before" column is the renderer's own
naive/accidental baseline, not the reference, and the deleted sort path WAS this renderer's. It is a
C-mode complexity argument over the author's own prior code, correctly framed.)*

### S4 — Abstract `abstract.tex:33-35` / Ch8 `08-conclusion.tex:21`: RIS "1.4×" understates the measured peak (UNDER-CLAIM)
> abstract: "volumetric product-resampled direct lighting, $1.4\times$ at equal quality under environment lighting"
> Ch8: "volumetric product-RIS direct lighting---$1.4\times$ at equal quality under environment lighting, default off"

**This is an under-claim, not an overclaim — flag for the author's awareness, do not force a change.** The
measured win is **1.475–1.492×** at K=6 on the showcase meadow (`ris_ksweep.md`: 1.475 banked / 1.492
re-anchor, bootstrap CI [1.467, 1.483]) and 1.445× on studio. Ch6 reports it precisely as "1.48×"
(`06-optimization.tex:139`). The abstract/conclusion round it *down* to "1.4×". For a de-hyped thesis a
conservative round is defensible and consistent (it never overstates), but the author should know 1.48× is
the defensible number and "1.4×" leaves ~0.08× on the table. If consistency with Ch6 is wanted, use "1.48×"
or "~1.5×"; if conservative rounding is the deliberate house style, leave it. **No action required** — noted
only so it is a conscious choice.

### S5 — Polish-adjacent superlative: Ch6 `06-optimization.tex:321` and Ch8 `08-conclusion.tex:22`: "rigorously"
> Ch6: "Cap-free streaming scatter sampling --- the most \emph{rigorously} killed."
> Ch8: "a \emph{rigorously} characterised ledger of negative results."

The cap-free autopsy *is* unusually thorough (bit-exact gate across furnace + 4 assets, 4-hypothesis profile,
root-caused a real bug — `06-optimization.tex:321-338`), so "rigorously killed" is earned in Ch6. The Ch8
"rigorously characterised ledger" is the filler-superlative form the brief asks to watch (CONVENTIONS: cut
"very"/intensifiers). Listed under Polish below; keeping the Ch6 one is fine.

---

## POLISH

### P1 — Ch8 `08-conclusion.tex:22`: "rigorously characterised ledger" → "characterised ledger" (filler).
### P2 — Abstract `abstract.tex:15` & Ch1 `01-introduction.tex:14`: "substantially" appears 4× thesis-wide
(abstract:15, Ch1:14, Ch1:22, Ch7:96, Ch8:46). All but Ch1:22 (see S2) are defensible — Ch7:96
("substantially faster than the reference's only unbiased configuration") is correctly scoped, Ch1:14 is
about DSYG's cost not the contribution, Ch8:46 is "substantially denser scene" (neutral). No change needed
beyond S2; just noting the word's frequency so it doesn't read as a tic.
### P3 — Ch4 `04-architecture.tex:188-191`, `:243-245`: "exactly"/"\emph{exactly}" is used 3× for the argmin
distribution match. The math (the independence product, `:196-209`) genuinely proves the argmin reproduces
the combined free-flight law exactly — this is a *mathematical* exactness claim, correctly distinguished from
the *empirical* ~10⁻⁴ residual (which is disclosed separately in Ch5/Ch8 as an implementation-level overlap
effect, candidate-caused). The two are not in conflict: the sampler is exact in expectation; a small residual
exists in the measured renders under heavy overlap. **This is correct and well-separated — do not touch.**
(See "Appropriately humble" §H4.)

---

## APPROPRIATELY HUMBLE — DO NOT TOUCH

H1. **Abstract is well-scoped throughout.** "roughly $59\times$ faster at equal quality" is explicitly tied to
"measured against the reference's only unbiased (analog) configuration" and "On the environment-lit showcase"
— exactly the scoping the brief demanded. "matches the unbiased ground truth to within Monte-Carlo noise"
is backed by `g1_headline.md` (+0.4% vs analog GT, inside noise floor) and the +0.4% is the genuine measured
figure. "free of that configuration's fireflies" is verified: ours-MIS raw/clip k = 1.99/1.887 = 1.05 (no
tail) vs Mitsuba-analog 3899/110.6 = 35× (heavy tail). "$578$ against $838$\,MiB on the same scene" is exact
(`vram.md`) and correctly carries "on less device memory" + "same scene". The RIS line carries both required
hedges ("scene-dependent", "under environment lighting").

H2. **Ch7 §7.1 "Isolating the sampler" and "The headline is environment importance sampling"** are a model of
honesty: states the core sampler is "~0.6×" net equal-quality on flat, calls the architectural contribution a
"\emph{structural, throughput} simplification ... \emph{not} a per-sample variance reduction", and says the
59× "is thus the payoff of importance-sampling a peaky HDR environment ... rather than a generic sampler win."
This exactly matches `g1_flat.md`. Nothing to change.

H3. **Ch5 scattering-ladder residual disclosure** (`05-validation.tex:214-224`) — the prior review's #1
CRITICAL ("disclose the overlap-scatter residual; repair the unbiasedness framing") is fully resolved. The
text now states the +2×10⁻⁴ cluster / +1×10⁻⁴ cloud overlap residual, "about six standard errors",
"convergence-stable", "below 0.2% of peak", names the candidate cause (argmin free-flight under overlap), and
frames unbiasedness as empirical "to within ~10⁻⁴" rather than absolute. Matches FINDINGS §8.3/§8.4 exactly
(§8.3 traits core +0.0002 @ ~6 SEM; §8.4 cloud T<0.4 core +1.0e-4 @ 3.3σ). This is the gold-standard fix.

H4. **Ch4 argmin "exact" vs Ch5/Ch8 empirical ~10⁻⁴** — the mathematical-exactness claim (proven, `:196-209`)
and the measured small residual (disclosed as a known limitation, Ch8:54-59, bounded "well under a part in
500") are correctly distinguished and never conflated. The Ch8 limitations bullet is exemplary: lists all
three residuals (overlap +2e-4, low-density +1.8e-4, coloured-albedo) with magnitudes and regimes.

H5. **Ch6 icosphere section** (`sec:icosphere`) — reports a result that goes *against* the renderer's choice
("The tessellated shell is \emph{faster at every tessellation level}: the analytic sphere pays 1.17–1.58×")
and keeps the analytic sphere "\emph{for correctness, at a now-quantified price}". This is the opposite of
overclaiming — a measured loss reported plainly. Correctly kept OUT of the abstract. Matches `icosphere_port.md`.

H6. **Ch7 §7.7 scaling** correctly restricts the sub-linear (N^0.40) claim to the controlled synthetic
square-grid family, explicitly refuses to read the production assets as a scaling curve ("deliberately
\emph{not} read as a scaling curve"), and attributes the steeper asset growth to "medium physics, not scene
size." Verified: square-grid exponent = 0.397, rectangles non-monotonic (disclosed). The "geometry cost is
sub-linear; residual is medium physics" framing is exactly what the data supports.

H7. **Ch7 §7.5 cross-renderer generalisation** uses the differential criterion honestly (mean ratios
0.9991/1.0001 PLUS unstructured-difference visual check, "by the criterion of §5.2 ... the signature of
Monte-Carlo noise, not a residual bias"), and discloses the bunny is NOT run against Mitsuba (ambiguous fit
variants) rather than hiding it. Matches `g1_headline.md` (bunny "ours-internal").

H8. **Ch5 voxel cross-check** correctly scopes itself to **absorption** and explicitly declines to claim a
scattering cross-check ("Scattering is deliberately not cross-checked this way: a dense-grid scattering
reference ... is either block-biased ... or firefly-limited"). Honest about the method's reach.

H9. **Wavefront / dead-end ledger** (Ch6 §sec:autopsies) — the "100–1400× slower" (FINDINGS §8.34: single-prim
~1400×), "~352 B/ray" (§8.34), and the A1/per-step-RB analysis are all faithfully reported, including the
correction that it's a "characterised trade-off, not a missed optimisation". The "fatal for this workload"
causal claim is supported by the profile (state must stream through DRAM, super-linear past L2).

---

## REGRESSION-CHECK vs prior review (`2026-06-10-full-review-ex-ch6.md`)

| Prior flag | Status now |
|---|---|
| #2 residual "boundary-sorting" (abstract:22, RW:54-57/76-77, concl:17-18) | **FIXED** — abstract & related-work & conclusion no longer attribute a sort to the reference (RW:55-56 = "running minimum"). One soft residual remains: Ch4:7 "sorted sequence" (S3). |
| #7 unqualified "novel" architecture | **PARTIALLY FIXED** — abstract corrected; Ch8:11 still bare "novel" (S1, the one real regression). |
| #7 RIS "win" unqualified | **FIXED** — abstract & Ch8 both carry "scene-dependent" + "under environment lighting". |
| #7 "measured across … memory" when memory unmeasured | **FIXED & RESOLVED** — memory IS now measured (578 vs 838, `vram.md`); abstract's "578 against 838 MiB" is real. |
| #1 disclose overlap-scatter residual + repair unbiasedness framing | **FIXED** (H3). |
| abstract "fivefold deficit … overtake" (wrong antecedent, flat-only) | **REMOVED ENTIRELY** — replaced by the scoped 59× framing. Good. |

No previously-fixed item was re-broken except the single-word "novel" in Ch8 (S1).

---

## Numbers re-derived from data (spot-check log)
- 59× clipped equal-quality: Mitsuba-analog k_clip 110.6 / ours-MIS k_clip 1.887 = 58.6 ✓ (`g1_headline.md`)
- 156% NEE bias: 0.8199/0.3201 − 1 = 156.1% ✓
- 31% memory: 1 − 578/838 = 31.0% ✓ (`vram.md`)
- 68× s.e. ratio (Ch5 fig caption): re-ran `scattering_convergence.py` on banked 16 seeds → ours σ 0.00023,
  Mitsuba-analog σ 0.01586, ratio 68.3× ✓; "agree to 0.4%" → 0.41% ✓. Note: this figure compares ours-**MIS**
  vs Mitsuba-**analog** (caption states this); it is a mean-estimate-tightening (s.e.) figure, NOT the
  equal-quality 59× — the two are distinct and the caption keeps them distinct. Honest.
- RIS 1.48× (K=6): 1.475 banked / 1.492 re-anchor, CI [1.467,1.483] ✓ (`ris_ksweep.md`)
- N^0.40: fitted square-grid exponent 0.397 ✓; 256×→9.7× (~10×) ✓ (`scaling.csv`)
- denoiser 7.2× RMSE (0.353→0.049) and 28–30× effective ✓ (`g2_denoiser.md`: 64×(0.178/0.034)²≈1788≈28×)
- furnace: Mitsuba NEE +6.5% (FINDINGS §8.1) ✓; ours flat ✓
- RR depth: dev ~11% (§8.33) → controlled +4.7% (basin 8–12, min 12) ✓; both stated, Ch6 notes the revision
- wavefront 100–1400× / 352 B/ray ✓ (FINDINGS §8.34)
- ours-MIS firefly-free: raw/clip k = 1.05 ✓ ("free of that configuration's fireflies" earned)
