# Examiner's review — Piotr Didyk (advisor / chair), folding the style-and-presentation standard

**Thesis:** *Efficient Volume Rendering Through Primitive-Based Kernel Mixture Volumes* (Kacper
Kramarz-Fernandez, MSc).
**Reviewer remit:** I read this as the advisor and de-facto chair — the senior generalist who judges the
thesis *as scholarship and as a finished professional artefact* — and I also carry the written-artefact
standard (P4). My owned dimensions, graded in depth: **5 (argumentation & significance), 7 (writing), 8
(presentation), 9 (cross-thesis consistency), 10 (defense-readiness)**, plus the overall verdict. I grade
1–4 (correctness, rigour, honesty, relevance) and 6 (related work) briefly, deferring the deep
correctness/systems interrogation to Condor and Talbot.

I verified every clock-independent load-bearing number against the campaign data and re-read the rendered
PDF for flow, figures, and tables. I did **not** edit the thesis source or touch git.

---

## Bottom line

This is a genuinely good thesis, and an unusually honest one. Its signature strength is **claim
calibration**: it consistently refuses to oversell. The argmin sampler that headlines the architecture is
disclosed to be a net *loss* at equal quality on its own (≈0.6×); the 59× is scoped, in the author's own
words, to peaky illumination "and honestly so"; the memory story is now stated as *mixed* (the renderer
loses on the dense assets); the 179× row is flagged ≈0.1 % biased; and the negative-results ledger is
sold — correctly — as a contribution in its own right. Every load-bearing figure I checked (the 59×, the
77/110/151/179 Ada ladder, bunny parity 0.9984, the scaling exponent, the VRAM table, the +156 % NEE bias)
reproduces to the digit. The figures are professional and self-contained, the prose is clear and
well-signposted, and the three structural Blockers from the June 15–16 reviews (sorting-misattribution,
the "began with profiling" false history, the absorption/scattering mislabel) are all resolved.

Where it is **weakest** is concentrated and fixable, and it clusters in my owned dimensions:

1. **The single most important argument in the thesis is asserted, not assembled.** The entire headline —
   and the whole Ada ladder built on it — rests on the premise that the reference's *only* unbiased mode
   is the noisy analog one, i.e. that its fast NEE estimator is intrinsically +156 % energy-biased on this
   medium. The thesis proves the *number* but never gives the *mechanism*, never establishes that no fast
   *unbiased* `volprim` configuration exists, and never assembles, at the point of attack, the furnace
   evidence that the bias is intrinsic. This is the question the defense will open on (see *Strongest
   objection*).
2. **A genuine cross-table contradiction** in the capacity story (`tab:overlap` says the bunny needs 245;
   `tab:vram` ships it at 80) with a caption that claims the estimator sizes the caps — which the thesis's
   own Chapter 4 narrative explicitly disputes.
3. **An internal-document leak** — a table column titled "FINDINGS" and ~14 inline `(§8.x)` pointers to a
   lab notebook the examiner cannot see and that resolve to no thesis section.
4. A scatter of **presentation and consistency nits** that a single focused pass would clear: the American
   "Optimization" in the Chapter 6 title, two uncited headline datasets, a converged-mean agreement quoted
   to a precision the displayed digits don't support, and over-repetition of the Ada/SER result.

**Verdict: minor revisions.** The thesis is comfortably MSc-worthy and would pass; none of the above is a
structural flaw or a rework. But item 1 is *minor as a document edit and major as a defense risk*: a
paragraph of mechanism plus an assembled cross-reference closes it, and it must be closed before the
defense, not improvised at the table.

---

## 1. Grades

| # | Dimension | Grade | One-line justification |
|---|-----------|:----:|------------------------|
| 1 | Technical correctness & soundness | **5** | All 8 load-bearing numbers reproduce to the digit; the argmin-exactness proof and the whitening derivation are clean and correctly attributed. (Loose complexity notation is a writing/consistency nit, dim 9.) |
| 2 | Experimental rigour & methodology | **4** | Bias/variance separation, interleaved A/Bs, equal-quality `k·t`, the independent voxel cross-check, and the M/C/S "what to measure" taxonomy are exemplary; a few soft denominators (flat-env Mitsuba time unrecorded, the one-time 100–1400× wavefront figure, the n=4 scaling fit) keep it from 5. |
| 3 | Honesty & claim calibration | **5** | The thesis's defining quality: it de-hypes its own headline contribution, scopes the 59× explicitly, flags the 0.1 % bias, and states the mixed memory result plainly. Exactly the register I want rewarded. |
| 4 | Relevance & scope discipline | **4** | Cleanly scoped (Gaussian-only, single-frame, deliberate). The one open question is significance *attribution* — the headline is carried by a standard MIS estimator plus the reference's limitation, not by the novel architecture (see dim 5). |
| **5** | **Argumentation, narrative & significance** | **4** | Strong arc (question → architecture → correctness → optimisation → results) and the negative-results ledger is sold as a real contribution. Held from 5 by two gaps: the headline is not *attributed* to the primary contribution, and the load-bearing "analog is the only honest baseline" premise is asserted rather than argued. |
| 6 | Related work & positioning | **4** | NeRF→3DGS→DSYG→(3DGRT/EVER now cited)→decomposition tracking→sort-free→reference renderers is a clean map. Docked for two uncited headline datasets and for putting measured startup numbers in the related-work chapter. |
| **7** | **Writing & academic style** | **4** | Clear topic sentences, disciplined signposting, economical de-hyped prose — near-excellent. Held from 5 by the American "Optimization" chapter title, pockets of informal register, and "fast-erf" terminology drift. |
| **8** | **Professionalism & presentation** | **4** | Figures are a real strength: legible, reader-facing, colour-disciplined, self-contained captions. Held from 5 by the `§8.x`/"FINDINGS" leak, the over-claiming `tab:overlap` caption, and the writing-prompt scaffold left commented in `abstract.tex`. |
| **9** | **Cross-thesis consistency** | **3** | The weakest owned dimension and the most cheaply fixed. Several real, reader-visible inconsistencies: the `tab:overlap`↔`tab:vram` cap contradiction; the complexity notation ($O(H)$ vs $O(H^2)$; "the reference's $N$" for $H$; $O(N{+}A)$ vs $O(A{+}H)$); the 0.4 % agreement vs the displayed 0.321/0.320; and the SER range printed as both 1.1–1.7 and 1.12–1.68. A dedicated pass lifts this to 5. |
| **10** | **Defense-readiness** | **3** | Largely ready — numbers reproduce, scoping is honest, prior Blockers are closed — but the single most likely opening question (the NEE-bias mechanism / why-not-`volprim`-MIS) has no assembled answer in the document. 3 now; 5 once that one section exists. |

**Median: 4. Overall verdict: minor revisions** — a strong, defensible thesis that needs one focused
revision pass, of which exactly one item (the NEE mechanism) is defense-critical.

---

## 2. Findings

Severity follows the brief: **Blocking** = unsupported load-bearing claim / contradiction; **Should-fix**
= overclaim, weak evidence, figure–caption or cross-thesis mismatch; **Polish** = wording, register,
typography. `file:line` is into `thesis/latex/`.

### Blocking

- **B1 — The load-bearing "the reference's only unbiased mode is analog" is asserted, not established;
  the +156 % bias has no mechanism.** `chapters/07-results.tex:100–103` ("*the reference's NEE result is
  \num{0.8199}, +156\,\% too bright … its only unbiased mode is therefore the analog one*") and
  `fig:g1-bias` (`:91`). The *number* reproduces exactly (0.8199/0.3201 = 2.561). But the claim the 59×
  depends on is the **negative** one — that no *fast unbiased* `volprim` configuration exists — and that is
  never shown, and the +156 % is never mechanistically explained. **Fix:** add a short paragraph in
  `sec:results-firefly` that (a) states the mechanism (most plausibly: `volprim`'s explicit NEE term is not
  MIS-weighted / not transmittance-correct under heavy overlap, so direct light is double-counted — the
  very failure this renderer's continuation-ray suppression and MIS weighting avoid, cf.
  `chapters/04-architecture.tex:314`); (b) records that `volprim` ran at its documented intended
  configuration (max_depth, kernel, solver); and (c) cites the furnace result (`chapters/05-validation.tex:208`,
  the reference-free +6.5 % over-estimate) *at this point of use* as independent evidence the bias is
  intrinsic, not imposed. The materials exist, scattered; they must be assembled where the claim is made.
  This is a one-paragraph document edit and the highest-leverage change in the thesis.

- **B2 — `tab:overlap` and `tab:vram` contradict each other on the bunny, and `tab:overlap`'s caption
  over-claims the estimator.** `tab:overlap` (`chapters/06-optimization.tex:543`) lists the bunny at *point
  overlap 245 / ray entries 387*, with a caption asserting "*point overlap sizes `MAX_ACTIVE_PRIMS`; ray
  entries size `HIT_BUFFER_CAPACITY`*" (`:546–547`). But `tab:vram` (`chapters/07-results.tex:166`) ships
  the bunny at **80/528**, and Chapter 4 (`chapters/04-architecture.tex:392–396`) explicitly says the
  estimator over-sized the bunny's active set four-fold (245 predicted vs 71 measured) and under-shot its
  hit count (387 vs 464), so "*the in-render counters, not the estimator, are the sizing authority*." A
  reader comparing the two tables sees 245 against 80 with no on-page reconciliation, and the caption
  asserts precisely the sizing role the narrative disowns. **Fix:** either add a *measured* column to
  `tab:overlap` (estimate vs measured vs shipped cap), or reword the caption to "*estimated $3\sigma$
  overlap; the in-render counters set the shipped caps where they differ (notably the bunny — see
  §4.6)*," and cross-reference `tab:vram`. The data are correct; the presentation manufactures a
  contradiction.

### Should-fix

- **SF1 — Internal-document leak: the "FINDINGS" column and ~14 `(§8.x)` pointers.** `tab:wins` carries a
  column literally headed *FINDINGS* with cells *§8.16, §8.23, §8.19, §8.18, §8.21*
  (`chapters/06-optimization.tex:103–110`), and the autopsy ledger is peppered with *(§8.34) (§8.29)
  (§8.27) (§8.30) (§8.31) (§8.32) (§8.35) (§8.36) (§8.20) (§8.38)* (`:329–390`). These resolve to an
  internal `FINDINGS.md`; Chapter 8 is the four-section Conclusion, so to an examiner they read as dangling
  cross-references. **Fix:** drop the column and strip every `(§8.x)`; where a pointer carries real
  information, convert it to an in-thesis `\Cref` or a one-line footnote. This is the most visible
  professionalism defect.

- **SF2 — Complexity notation is internally inconsistent.** The reference's per-bounce cost is "$O(H)$
  traversals" at `chapters/04-architecture.tex:75` but "$O(H^2)$ running-minimum selection" at `:298` and
  in `tab:complexity` (`chapters/06-optimization.tex:210`); these are reconcilable ($H$ traversals, $O(H^2)$
  comparisons) but never reconciled in text. "*One traversal therefore replaces the reference's $N$*"
  (`:89`) should read $H$, not the scene-size $N$. And this renderer's per-bounce cost is "$O(N+A)$" in
  prose (`:295`) but "$O(A{+}H)$" in `fig:pipeline` (`:56`), with $H$ silently dropped from the prose
  total. **Fix:** state once that the reference is $H$ traversals → $O(H^2)$ ordered comparisons; correct
  `:89` to $H$; and make the figure/prose agree on $O(N + A + H)$ for the base and $O(A+H)$ for the
  active-set-inheriting optimised path.

- **SF3 — Converged-mean agreement quoted more precisely than the displayed digits support.** The text
  claims "*agreeing to \num{0.4}\,\%*" (`chapters/05-validation.tex:230`) while the same caption and figure
  show 0.321 vs 0.320 (≈0.3 %); the showcase caption shows "*0.321 versus 0.321*"
  (`:280–281`, i.e. 0 %); and `fig:g1-bias` prints "+0.4 %" beside "0.321 / 0.320". The underlying figures
  are fine — the *legend* of `scattering_ladder` already uses 0.3214/0.3201, which *does* give 0.4 % — but
  the prose rounds to three significant figures, which cannot carry the claim. **Fix:** report the means to
  four significant figures (0.3214 / 0.3201) everywhere the 0.4 % appears.

- **SF4 — Chapter 6 title uses American spelling.** `chapters/06-optimization.tex:1`,
  `\chapter{Performance Engineering and Optimization}` — the one British-spelling miss, and the most
  visible, since it propagates into the running header and the table of contents of a thesis that mandates
  `-isation`. **Fix:** *Optimisation*.

- **SF5 — Two uncited headline datasets.** The Disney/WDAS cloud ("*Gaussians fit to the Disney/WDAS cloud
  volume*", `chapters/05-validation.tex:174`) and the Stanford bunny (`:543`, `chapters/07-results.tex:206`)
  are both named, attributable assets with ready `refs.bib` entries (`WDASCloud2017`, `TurkLevoy1994`) that
  are never cited. Citing the source of the asset used "throughout this thesis" is basic scholarship.
  **Fix:** `\cite` both at first mention.

- **SF6 — "fast-erf" terminology drift, and the table label is undefined before use.** The same feature is
  "fast-math $\operatorname{erf}$" (`chapters/04-architecture.tex:419`), "Fast \texttt{erf}"
  (`chapters/06-optimization.tex:110`), "approximate-erf" (`:474`), and "fast-erf" (`:492`, `:500`); the
  `tab:ser-eq` row label *RIS + fast-erf + SER* uses a name never defined. **Fix:** choose one canonical
  term, define it once, and use it in the table.

- **SF7 — The ~3× per-sample claim rests on an unrecorded denominator.** "*this renderer is $\sim$3×
  faster per sample (\SI{2.90}{\second} versus $\sim$\SI{8.5}{\second})*"
  (`chapters/07-results.tex:40–44`), with the footnote conceding "*the flat-env Mitsuba-analog frame time
  was not separately recorded; \SI{8.5}{\second} is the meadow-analog steady state reused*." The
  "if anything conservative" reasoning is plausible, but a headline ratio with an unmeasured arm is exactly
  what Talbot will press. **Fix:** record the flat-env Mitsuba time, or present the ~3× explicitly as a
  lower bound.

- **SF8 — The Ada/SER result is over-repeated, and its precision flips.** The 1.1–1.7× SER figure appears
  in the abstract (`abstract.tex:38`), the intro contributions bullet (`chapters/01-introduction.tex:44–46`),
  a *standalone* intro paragraph immediately after it (`:49–50`), and three times in the conclusion
  (`chapters/08-conclusion.tex:32`, `:60–62`, `:85`) — and is printed as both "1.1–1.7" and "1.12–1.68".
  For a deliberately *scoped, corroborating* probe, this much drum-beating slightly undercuts the "does not
  replace the 59×" framing. **Fix:** cut the redundant intro paragraph (`:49–50`, which merely restates the
  bullet above it) and fix the range to one precision.

- **SF9 — The showcase frame-time juxtaposition can be misread as the per-frame advantage.** The showcase
  caption gives "*our frame $\approx$\SI{9}{\second}; the Mitsuba-analog frame $\approx$\SI{13.6}{\second}
  wall, including JIT start-up*" (`chapters/05-validation.tex:280`). Chapter 7 establishes that on the 3090
  the steady-state per-sample times are essentially equal (the 59× is variance, not speed — the headline
  "*most of the factor coming from the estimator's lower variance rather than from raw per-sample speed*",
  `chapters/06-optimization.tex:478`). A reader meeting 9 vs 13.6 first will infer a 1.5× per-frame gap that
  the results chapter then contradicts. **Fix:** note in the caption that the 13.6 s is startup-inflated and
  that steady-state per-frame times are comparable.

### Polish

- **P1 — Register lapses.** "*the big one*" (`chapters/06-optimization.tex:329`), "*the most rigorously
  killed*" (`:339`), the contraction "*don't*" (`:70`), and the figure caption "*Ours snaps to it from the
  first seed; the firefly-prone analog wiggles in*" (`chapters/05-validation.tex:231`) break an otherwise
  sustained formal register. Normalise (e.g. "the largest regression", "the most thoroughly disproven",
  "do not", "converges from the first seed; the analog approaches it gradually").
- **P2 — "artifact" vs "artefact".** `chapters/06-optimization.tex:312` ("*sliver artifact*") is American;
  the thesis uses British "artefacts" at `05:31` and `07:95`. Standardise.
- **P3 — "Monte-Carlo noise" hyphenation.** The compound modifier is hyphenated ("Monte-Carlo noise", 7×)
  against the body's `Monte~Carlo`. Pick one.
- **P4 — BRDF not introduced.** `chapters/02-background.tex:61` spells out "bidirectional reflectance
  distribution function" without "(BRDF)", then "the BRDF" is used at `:118`. Add the parenthetical.
- **P5 — Writing-prompt scaffold left in source.** `abstract.tex:2–6` still carries the commented
  `% What is my topic? …` template prompts. Harmless but should be deleted before submission.
- **P6 — `tab:wins` "Effect" column mixes units.** It lists a *shadow-kernel* 12–15× beside *% frame*
  wins (`chapters/06-optimization.tex:105–110`); the "(shadow kernel)" tag is honest but a reader can
  misread the 12–15× as a frame win. Split or annotate the column.
- **P7 — Orphan bib entries.** Several entries are defined but never cited (`PBRT4`, `Vitter1985`,
  `Chao1982`, …). Cite `PBRT4` for the Chapter 2 background (a natural reference) or prune the dead
  entries.
- **P8 — Cosmetic overfulls.** Six overfull \hbox, the widest being `tab:icosphere` (7.74 pt) and
  `tab:overlap` (5.50 pt); tighten `\tabcolsep` or `\small`. Zero undefined references.

---

## 3. Structural recommendations

- **Move the end-to-end Ada equal-quality ladder (`tab:ser-eq` + "From a lever to an end-to-end
  advantage", `chapters/06-optimization.tex:462–507`) into Chapter 7, beside the 59× headline.** Keep the
  SER *mechanism* in §6.9 — that is a deliberate, well-placed optimisation section — but the 77/110/151/179
  *equal-quality results* are results, and the reader currently meets the 59× in Ch7, the ladder in Ch6,
  then returns to Ch7. Co-locating all equal-quality numbers in `sec:results-perf` keeps the headline and
  its corroboration in one place and reinforces "corroborates, does not replace."
- **Consolidate the two memory discussions.** §6.10 ("Memory", per-asset capacity budgets, `tab:overlap`)
  and §7.4 ("Memory footprint", cross-renderer `tab:vram`) overlap substantially — both rehearse the
  per-ray-state-dominates argument and the 578-vs-838 comparison. Reduce §6.10 to the one point the
  wavefront autopsy actually needs (per-ray state is the load-bearing memory term) and let §7.4 own the
  cross-renderer footprint. This also gives B2 a single home to fix.
- **Demote the measured startup numbers in related work to a forward reference.** Chapter 3 states the
  0.8 s / 2.2 s / 0.4 s startup measurements as motivation (`chapters/03-related-work.tex:101–103`); these
  are results. Replace with the qualitative motivation and a forward `\Cref{sec:results-startup}`.
- **Keep, do not cut, the "Isolating the sampler" / "headline is environment importance sampling"
  paragraphs (`chapters/07-results.tex:34–62`).** They are the honest heart of the results and the main
  reason I trust the rest of the document; if anything, foreshadow them once in the introduction so the
  reader knows from the outset that the architecture is a *throughput/structural* contribution and the
  equal-quality win is the estimator.
- **Optional merge:** the short §6.6 "Reasoned decisions, not ablated" (`:225`) is a one-paragraph bridge
  that could fold into the taxonomy (§6.2) or open §6.7 directly.

---

## 4. Strongest objection (the question I would open the defense with)

*"Your entire headline — the 59×, and the 77/110/151/179 Ada ladder built on top of it — rests on one
premise: that the reference's only unbiased configuration is its noisy analog mode, because its fast NEE
estimator is +156 % too bright on this medium. You have proved the number. You have not told me why. A
+156 % energy bias in a mature, published renderer — your co-advisor's own `volprim` — is so large that my
first instinct, and any external examiner's, is that it was misconfigured: a wrong max-depth, an MIS flag
left off, a kernel or solver mismatch. If that bias is a setup artefact rather than an intrinsic property
of the estimator, then the honest baseline is not analog but a fast unbiased `volprim` mode, and the 59×
collapses toward the ≈0.6× your own argmin sampler achieves on its own. So: what is the mechanism, and how
do I know there is no fast unbiased configuration of the reference you should have compared against
instead?"*

The thesis can pre-empt this, and the pieces are already in the building — they are simply not assembled
where the claim is made. The defense answer needs three moves, in `sec:results-firefly`: **(1) the
mechanism** — state it explicitly (the most defensible reading is that `volprim`'s explicit next-event
term is not MIS-weighted and not transmittance-correct under heavy primitive overlap, so direct light is
double-counted; this is exactly the failure the renderer's own continuation-ray suppression and MIS
weighting are designed to avoid, `chapters/04-architecture.tex:314`); **(2) the configuration record** —
that `volprim` ran at its documented intended settings, so the bias is the estimator's, not the operator's;
and **(3) the independent corroboration** — cite the furnace test (`chapters/05-validation.tex:208`), a
*reference-free* energy invariant on which the same NEE estimator over-shoots by +6.5 %, at the point of
use, as proof the bias is intrinsic and not an artefact of the showcase. With those three sentences in
place, the most dangerous question in the room becomes the thesis's strongest moment; without them, a
verified number is left looking like a misconfiguration.

A quieter second objection, which is mine as chair rather than Condor's, belongs in the same revision:
**attribute the headline correctly.** Your *novel* contribution is the single-trace/argmin architecture;
your *headline* (59×) is delivered by a textbook MIS estimator plus the reference's bias, and your own
Chapter 7 shows the architecture is ≈0.6× at equal quality on its own. That is honest, and I commend it —
but it means the significance of the *contribution* must be argued on its true grounds: that it is
march-/sort-/root-find-free and *structurally* faster per sample; that it is what makes the megakernel
shape, and therefore the SER lever, available at all; and that the negative-results ledger maps the
performance ceiling of the whole class. Argue the architecture's worth there, where it is real, and let
the 59× be honestly what it is — a statement about importance-sampling a peaky environment that the
reference's only unbiased mode cannot. Do that, and the de-hyping that is this thesis's best quality
becomes its strongest defense rather than a concession.
