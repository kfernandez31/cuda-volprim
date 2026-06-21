# Examiner's review (round 2) — Piotr Didyk (advisor / chair), folding the style-and-presentation standard

**Thesis:** *Efficient Volume Rendering Through Primitive-Based Kernel Mixture Volumes* (Kacper
Kramarz-Fernandez, MSc).
**Reviewer remit:** I read this as the advisor and de-facto chair — the senior generalist who judges the
thesis *as scholarship and as a finished professional artefact* — and I also carry the written-artefact
standard (P4). Owned dimensions, graded in depth: **5 (argumentation & significance), 7 (writing), 8
(presentation), 9 (cross-thesis consistency), 10 (defense-readiness)**, plus the overall verdict; 1–4 and 6
graded briefly, deferring the deep correctness/systems interrogation to Condor and Talbot.
**Round 2 job:** *verify* that iteration-1 fixes are correct and complete, and bring *fresh eyes*. I
re-read the full rendered PDF and the `.tex` source, re-derived every clock-independent load-bearing number
against the banked campaign data, and confirmed the build (`latexmk`) is clean. I did **not** edit the
thesis source or touch git.

---

## Bottom line

**The two round-1 Blockers are closed, verified, and reproduce from banked data; the professionalism sweep
is essentially complete; and my own round-1 second objection — attribute the headline correctly — is now
answered in the document.** The single most dangerous question in the June-20 defense prep (the missing
mechanism for the reference's +156 % NEE bias) now has an assembled, on-page, banked answer at the point of
use. That moves the thesis from "defensible but with one improvised answer" to "defense-ready."

Concretely, I verified:

- **B1 (NEE-bias mechanism) — CLOSED.** §7.2 now carries the mechanism (a sampling-measure mismatch:
  analytic-deterministic shadow-ray transmittance MIS-combined against a stochastic analog-survival
  continuation on bare directional densities → the partition is inexact → a surplus survives at interior
  vertices; ratio-tracking would remove it), the configuration record (Gaussian kernel, bisection,
  depth 128 "deep enough that truncation can only darken"), and the optical-thickness magnitude bridge
  (centre over-count ≈1 % → ≈10 % → ≈30 % at σ = 2/6/12, compounded by overlap to +156 % on the cloud).
  §5.5 frames the furnace as the reference-free, depth-invariant proof and forward-references the mechanism.
  The banked `furnace.md` reproduces exactly (0.86 % / 9.74 % / 30.94 %); `s4_erfinv.md` and `g1_headline.md`
  reproduce their numbers to the digit. **This is the highest-leverage change in the revision and it has
  landed.**
- **B2 (`tab:overlap` ↔ `tab:vram`) — CLOSED.** The `tab:overlap` caption now states the bunny chain
  explicitly (estimator 245 → measured 71 → shipped 80; entries 387 → measured 464 → shipped 528),
  cross-references `tab:vram`, and disowns the "estimator sizes the caps" over-claim ("where the in-render
  counters disagree, the counters are the sizing authority"). §4.6 says the same. No reader now meets 245
  against 80 unreconciled.
- **Headline 59× — now bounded.** "≈59× (95 % bootstrap CI [54, 63] over the 16 seeds)"; the footnote names
  the clip operation precisely ("per-pixel radiance clipped at a single global 99.9th-percentile threshold
  *before* variance … the most conservative of the firefly-discounting conventions"), flags the figure as
  variance-dominated, and points to the Ada row as the clean timing anchor.
- **Condor reference-accuracy (S1–S4) — CLOSED/decided.** DSYG is now characterised correctly everywhere
  (closed-form for single-Gaussian segments, bisection only in overlaps; the argmin extends closed-form
  sampling *across* overlaps); credit is scoped to the sampler half in intro and conclusion; kernel
  generality distinguishes the forward optical depth (both kernels) from the practical inverse
  (Gaussian-only); the heavy-overlap residual is decided against single-precision inversion.
- **Professionalism sweep — done.** The `FINDINGS` column and every `(§8.x)` pointer are gone (0 hits);
  "Optimisation" in the chapter title, header, and ToC; WDAS cloud and Stanford bunny cited; GPU terms fixed
  (registers + per-thread local memory, no "shared memory", no "coalesced" per-thread buffer, "256-bucket,
  8-bit" SER key); SER unified to 1.12–1.68×; means to four significant figures; RR penalty +5.0 %; the
  build now reports **0 overfull hboxes and 0 undefined references**; the abstract scaffold comments are
  gone; and the significance attribution is foreshadowed in the introduction.

**Where it is now weakest** is genuinely minor and almost entirely cosmetic. The residual items are: one
surviving complexity-notation inconsistency (`O(N+A)` written where the breakdown is `O(N)+O(A+H)`); the
"fast erf" term still drifts in styling and is never defined; a one-clause redundancy and a 30 %→31 %
rounding in the new §7.2 mechanism paragraph; and the deferred-polish set (Monte-Carlo hyphenation, orphan
bib entries, the Ch3 startup numbers, flat-rung 2.90/2.85 s). None is defense-critical; none is a rework.

**Verdict: minor (polish-only) revisions — no remaining Blockers; defensible as it stands.** This was
"minor revisions" in round 1 with exactly one defense-critical item; that item is now closed and verified.
What remains is a single afternoon's styling pass. I would pass this thesis.

---

## 1. Grades

| # | Dimension | R1 | **R2** | One-line justification |
|---|-----------|:--:|:--:|------------------------|
| 1 | Technical correctness & soundness | 5 | **5** | All load-bearing numbers reproduce to the digit; the NEE mechanism is now stated and the heavy-overlap residual is decided against single-precision inversion (`s4_erfinv.md`: 2.4×10⁻⁸). Deep correctness deferred to Condor. |
| 2 | Experimental rigour & methodology | 4 | **4** | The headline now carries a bootstrap CI and a precisely-named clip convention, the furnace is banked, and the bias mechanism is argued — closing the round-1 rigour gap. Still a strong-4: the ~3× per-sample and flat-rung denominators are reused/unpinned (honestly caveated as lower bounds), and the n=4 scaling fit and one-time wavefront figure remain. |
| 3 | Honesty & claim calibration | 5 | **5** | Unchanged signature strength: bare sampler reported as a net loss (~0.6×), 179× row flagged ~0.1 % biased, memory stated as *mixed*, 59× scoped to peaky illumination, negatives sold as contribution. The de-hyping is the register I want rewarded. |
| 4 | Relevance & scope discipline | 4 | **4** | Cleanly scoped (Gaussian-only, single-frame, deliberate); kernel-generality now correctly distinguishes forward integral from inverse. Held from 5 only by the (now honestly-stated) fact that the novel architecture is not what carries the headline. |
| **5** | **Argumentation, narrative & significance** | 4 | **5** | Both round-1 holds are fixed: the headline is now *attributed* to the MIS estimator (intro foreshadow + the "Isolating the sampler" / "headline is environment importance sampling" paragraphs), and "analog is the only honest baseline" is now *argued* via the mechanism, not asserted. The negative-results ledger is sold as a real contribution. This is now a model of de-hyped argumentation. |
| 6 | Related work & positioning | 4 | **4** | DSYG now characterised correctly; NeRF→3DGS→DSYG→(3DGRT/EVER)→decomposition-tracking→sort-free map is clean. Docked only for the Ch3 startup measurements still sitting in related work as motivation (a results number — see structural rec). |
| **7** | **Writing & academic style** | 4 | **4** | A *high*-4, one pass from 5. The British-spelling miss, the informal register ("the big one", "wiggles in", "don't"), and the worst terminology drift are all fixed; prose is clear, signposted, economical, de-hyped. Held from 5 only by the residual "fast erf" styling/definition, one redundant sentence in §7.2, and the Monte-Carlo hyphenation split. |
| **8** | **Professionalism & presentation** | 4 | **5** | The defect that held it at 4 — the `§8.x`/`FINDINGS` internal-doc leak — is gone; figures remain legible, reader-facing, and self-contained; captions are load-bearing; the build is clean (0 overfull hboxes, 0 undefined refs); headline datasets are cited. The only blemishes are deferred-trivial (7 orphan bib entries; the 7-row `tab:overlap` Condor wanted trimmed). |
| **9** | **Cross-thesis consistency** | 3 | **4** | The largest single jump. B2, the converged-mean precision (0.3214/0.3201), the SER range (1.12–1.68 throughout), and the RR magnitude (+5.0 %) are all reconciled; the complexity notation is *mostly* reconciled. Held from 5 by one residual (`O(N+A)` vs its own `O(N)+O(A+H)` breakdown), the abstract-0.4 %-vs-money-shot-1 % framing, and a 13.5/13.6 s wall-time mismatch. A focused pass reaches 5. |
| **10** | **Defense-readiness** | 3 | **5** | The single most likely opening question (the NEE-bias mechanism / why-not-`volprim`-MIS) now has an assembled, banked, on-page answer at the point of use, with the furnace played as the reference-free proof. Numbers reproduce, scoping is honest, all prior Blockers are closed. The candidate must still *rehearse* the mechanism aloud (with the ratio-tracking/`prbvolpath` contrast ready), but the document arms them fully. |

**Median: 4.5. Overall verdict: minor (polish-only) revisions — defensible as it stands.** A strong,
honest, MSc-worthy thesis whose one defense-critical gap has been closed and verified; what remains is
cosmetic.

---

## 2. Findings

Round 2, so this section is mostly **verification of closure** plus a short residual ledger. Severity per
the brief. `file:line` is into `thesis/latex/`.

### Blocking

**None.** Both round-1 Blockers (B1 NEE-mechanism, B2 cap-contradiction) are closed and verified against
banked data (see Bottom line). I re-ran the verification and could not reopen either.

### Should-fix (residual / fresh-eyes — all low severity)

- **SF1 — Residual complexity-notation inconsistency (a leftover of round-1 SF2).**
  `chapters/04-architecture.tex:297` writes the base per-bounce cost as `$O(N + A)$`, then in the same
  sentence breaks it down as "*an $O(N)$ scan … plus an $O(A + H)$ pass*" — i.e. `O(N) + O(A+H) =
  O(N+A+H)`. The leading total silently drops the `$H$` term that the breakdown and `fig:pipeline`
  (`:57`, `O(A+H)`) both carry. The "reference's N→H" and `O(H)`/`O(H²)` parts of round-1 SF2 *were*
  fixed; this one survived. **Fix:** write the base as `$O(N+A+H)$` (matching the breakdown) and keep the
  optimised path `$O(A+H)$`; or, if the intent is the loose asymptotic class, drop to `$O(N)$` and say so.
  *Quote:* "Per scattering bounce the work is $O(N + A)$: an $O(N)$ scan … plus an $O(A + H)$ pass".

- **SF2 — "fast erf" is still not unified and is never defined.** The egregious round-1 drift
  ("fast-math erf", "approximate-erf") is gone, but four stylings remain: "a fast \texttt{erf}"
  (`chapters/04-architecture.tex:421`), "Fast \texttt{erf} in the hot path" (`chapters/06-optimization.tex:110`),
  "the fast-\texttt{erf} hot path" (`:475`), and the bare "fast-erf" in `tab:ser-eq` (`:493`, `:501`).
  None of them says *what* the fast erf is (a hardware/polynomial approximation to `erf`). **Fix:** choose
  one form (e.g. "fast \texttt{erf}"), use it everywhere including the table, and add a half-clause at
  first mention (`04:421`) defining the approximation and noting it is numerically validated inside the
  gates. *Quote:* "RIS $+$ fast-erf $+$ SER" (`tab:ser-eq`).

- **SF3 — The abstract's "0.4 %" reads against the money-shot's "~1 %".** The abstract says the renderer
  "*matches the unbiased ground truth in the mean (to $0.4\%$…)*" (`abstract.tex:24`), while §5.7 says it
  "*agrees with the reference to within ${\sim}1\,\%$ across four well-separated views*"
  (`chapters/05-validation.tex:275`). Both are correct — 0.4 % is the single converged showcase view
  (`fig:g1-bias`), ~1 % the four-view envelope — but a reader meets them as two different correctness
  figures with no signpost. **Fix:** in the abstract, pin the 0.4 % to "the showcase view" (one word), so
  the two are visibly the single-view and the multi-view statements of the same result.

### Polish

- **P1 — One-clause redundancy in the new §7.2 mechanism paragraph.** "*the same over-count appears,
  reference-free, in the furnace test*" (`chapters/07-results.tex:114-115`) and, ten lines later, "*The
  same over-count is visible, and reference-free, in the furnace of \Cref{par:furnace}*" (`:124`) restate
  the furnace's reference-free status twice within one paragraph. Tighten the second to "*The same growth
  is visible in the furnace…*".
- **P2 — Furnace magnitude rounds 30.94 % to "≈30 %".** `chapters/07-results.tex:127` says "*${\approx}30\%$
  at 12*"; `furnace.md` banks 30.94 %, which is marginally better rendered "≈31 %" (and matches the
  campaign note's "~1 % → ~31 %"). Trivial, but the faithful round is upward.
- **P3 — Showcase wall-time 13.6 vs banked 13.5.** `chapters/05-validation.tex:288` quotes the
  Mitsuba-analog wall at "*${\approx}\SI{13.6}{\second}$*"; `g1_headline.md:12` banks "wall 13.5".
  Reconcile to one value.
- **P4 — Deferred-polish set (accepted, flagged for completeness, none defense-relevant).** Monte-Carlo
  hyphenation is split 7 (hyphenated) / 12 (not) across chapters; seven orphan bib entries remain
  (`PBRT4`, `Vitter1985`, `Chao1982`, `Enoki`, `RTI1W`, `ChineseDragon`, `SpatialAccelerationStructures`)
  — cite `PBRT4` for the Ch2 background or prune; flat-rung 2.90 s vs banked 2.85 s; the `fig:rr-depth`
  caption "16 seeds per depth" overstates the single-seed timing axis. I agree these are deferrable and do
  not contest the deferral.

---

## 3. Structural recommendations

The round-1 structural calls split into *deferred-by-choice* (I accept) and *still-open* (I re-raise, all
low priority):

- **Accept the deferral of `tab:ser-eq` relocation and the §6.10/§7.4 memory consolidation.** The bias
  story is now coherently split (mechanism + furnace framing in §5.5/§7.2; ladder in §6.9; cross-renderer
  memory owned by `tab:vram` in §7.4; per-asset budgets + `tab:overlap` in §6.10). Co-locating the Ada
  ladder beside the 59× would still read marginally better, but the current placement is defensible and the
  "corroborates, does not replace" framing is consistent. Not worth a reflow at this stage.
- **(Re-raise, minor) Demote the Ch3 startup numbers to a forward reference.** §3.5 still states the
  measured 0.8 s / 2.2 s / 0.4 s startup figures as motivation (`chapters/03-related-work.tex:101-103`);
  these are results, fully developed in §7.5. Replace with the qualitative motivation and a
  `\Cref{sec:results-startup}`. This is the one round-1 structural rec that was neither done nor formally
  deferred.
- **(Optional, Condor's call) Trim `tab:overlap` to the load-bearing rows.** The seven-row table makes the
  "overlap ≠ primitive count" point with four density fits; two (primary + the dense 4096 fit) make it as
  forcefully. The point is excellent; the table is heavier than the point needs. Now that the caption also
  carries the B2 reconciliation, trimming would also lighten a dense caption.
- **Keep, do not cut**, the "Isolating the sampler" / "headline is environment importance sampling"
  paragraphs (`chapters/07-results.tex:39-67`) and the negative-results ledger. These are the honest heart
  of the document and the reason the rest is credible. The intro now foreshadows them, exactly as the
  significance argument needs.

---

## 4. Strongest objection (the question I would open the defense with)

With the NEE-mechanism question — last round's opener — now answered in the document, the sharpest
remaining question is mine as chair, and it is the natural successor: **a question about the significance of
the *novel* contribution, stripped of the estimator and the reference's bias.**

*"Let us be precise about what is yours. The number this thesis is organised around — the 59× — is delivered
by a textbook MIS direct-lighting estimator exploiting a peaky environment, against a reference whose only
unbiased mode is its noisy analog one; your own Chapter 7 shows that your novel architecture, the
single-trace/argmin sampler, is a net ≈0.6× at equal quality on its own. So strip away the estimator and
strip away the reference's bias. What, quantitatively, is the architecture worth? You answer 'a structural,
throughput simplification — march-, sort-, root-find-free, ~3× faster per sample.' Good — but that ~3× rests
on a Mitsuba reference frame time your own footnote admits was reused from a different environment and never
cleanly pinned, so the one number that quantifies your actual contribution is the softest denominator in the
thesis. Convince me the architecture clears the MSc bar on its own terms, and tell me the defensible figure
for its per-sample structural advantage."*

This is the most dangerous remaining question because it goes to the heart of an MSc examination — *what did
the candidate contribute* — and because the honest answer the thesis has chosen (de-hyping the headline)
deliberately points the reader's attention away from the architecture and toward the estimator. The thesis
has, to its great credit, already assembled most of the rebuttal; the candidate needs to deliver it as one
confident, three-legged argument:

1. **The per-sample structural speedup is real and is the architecture's worth** — march-, sort-, and
   root-find-free, one traversal replacing `H`, an unordered argmin replacing the `O(H²)` boundary march
   and the overlap-regime root-find. The candidate should give this as the cleanest framing and *pre-empt*
   the soft denominator: present the ~3× explicitly as a *lower bound* (the thesis already says "if anything
   conservative"), and, if a clean steady-state Mitsuba 3090 render can be pinned before the defense, do so
   — it converts the thesis's softest input into its cleanest.
2. **The architecture is what makes the rest possible.** The megakernel shape it forces is not incidental:
   it is the load-bearing choice the wavefront and cap-free autopsies prove (externalising per-ray state is
   fatal), and it is precisely what gives the renderer the SER lever the reference's compiled Dr.Jit
   pipeline *cannot* reach — a genuine architectural advantage, measured at 1.12–1.68× image-identical on
   Ada and corroborated in the end-to-end ladder. The estimator wins are *available* because the
   architecture is shaped to host them.
3. **The negative-results ledger is itself the contribution the field lacks** — a profiled map of where
   effort on this class of renderer does and does not pay off (megakernel-shaped, latency-bound, resistant
   to the restructurings that help conventional path tracers). That map, plus the differential-validation
   methodology that doubles as the sampler's unbiasedness evidence, is MSc-worthy independently of any
   speedup.

Delivered that way — architecture worth = structural per-sample speedup *plus* the megakernel/SER
enablement *plus* the negative-results map, with the 59× honestly handed to the estimator — the question
that looks like it exposes a thin contribution instead demonstrates exactly the calibrated honesty that is
this thesis's best quality. The only homework is to pin, or explicitly bound, the per-sample denominator
before the table, so the architecture's one quantitative claim is as well-bounded as the 59× it is careful
not to take credit for.

---

*Overall (Didyk's view): a genuinely good and unusually honest thesis that has done the round-1 revision
properly. The one defense-critical gap is closed and verified; the contribution is clearly delineated and
honestly attributed; the writing and presentation are professional and now nearly clean. Pass with minor
(polish-only) revisions. Close SF1–SF3 and the §7.2 redundancy, pin or bound the per-sample denominator,
and rehearse the NEE mechanism aloud — and this defends comfortably.*
