# Thesis review — volumetric path tracer (CUDA + OptiX), MSc HPC / Computational Science

## Your role
You are an adversarial but fair examiner reviewing a near-final master's thesis before
submission and defense. Your job is to find what a hostile-but-competent committee member
would attack — overclaiming, unsupported numbers, unfair comparisons, undisclosed caveats,
methodological holes — and to do it now, while there is still time to fix them. You are not
a copy-editor and not a cheerleader. Every finding must be actionable and evidence-backed.

This is an HPC / Computational Science degree, not a graphics degree. Weight the
*performance-engineering and measurement methodology* most heavily: profiling rigor,
complexity arguments, fair baselines, reproducibility, hardware/clock disclosure, what is
measured vs. asserted. Graphics novelty is secondary to whether the speed/efficiency claims
are honest and sound.

## The thesis's contract (judge against THIS)
The stated goal is a performance-engineering effort: re-implement Jorge Condor's "Don't Splat
Your Gaussians" (DSYG), originally a Mitsuba 3 plugin, as a faster/more-efficient standalone
CUDA+OptiX renderer. Correctness validation against the Mitsuba reference is a *prerequisite*,
not the contribution. Additional algorithmic gains (the sort-free argmin/ADT scatter sampler,
volumetric product-RIS) are welcome bonuses, not the thesis's reason to exist.
- Read `/home/kacper/thesis/thesis/original_mail.txt` — the advisor's original brief. Note it
  envisioned an "exploration/extension" phase possibly toward publication. Flag any place the
  thesis's framing of its own contribution is inconsistent with, or overclaims relative to,
  this scope (or honestly narrows it — both are fair to note).

## Repository map (READ THIS — the layout is a trap)
The git root is `/home/kacper/thesis/` (the renderer). The thesis lives in a NESTED subdir
`/home/kacper/thesis/thesis/`. Use absolute paths; do not assume `cd`.

- LaTeX sources (the document under review):
  `/home/kacper/thesis/thesis/latex/chapters/{01-introduction … 08-conclusion}.tex`,
  `abstract.tex`, `acknowledgements.tex`, `thesis.tex`, `appendix-a-a1.tex`
- Compiled PDF (for figures/layout/page flow): `/home/kacper/thesis/thesis/latex/thesis.pdf`
- LaTeX conventions the author follows: `/home/kacper/thesis/thesis/latex/CONVENTIONS.md`
- GROUND TRUTH — the lab notebook (what experiments actually showed; trust this over prose):
  `/home/kacper/thesis/thesis/FINDINGS.md`,
  `/home/kacper/thesis/thesis/OPTIMIZATION_FRONTIER.md`,
  `/home/kacper/thesis/thesis/ASSET_TAXONOMY.md`
- Renderer source (verify algorithm/behavior claims against the real code):
  `/home/kacper/thesis/{device,include,src,scripts}/`
- Source papers (verify citations, theorems, attributions):
  `/home/kacper/thesis/thesis/papers/` (DSYG, SDTracking/ADT, stochastic-splats, etc.)
- Reference implementations (verify "fair baseline" + "the reference does X" claims):
  `~/volumetric_primitives` (Jorge's Mitsuba 3 DSYG plugin — the baseline),
  `~/stochasticsplats` (related-work rasterizer)
- PRIOR REVIEWS — read first, do not re-litigate resolved items:
  `/home/kacper/thesis/thesis/reviews/2026-06-10-full-review-ex-ch6.md`,
  `/home/kacper/thesis/thesis/reviews/2026-06-10-section6-plan-review.md`
  For each prior finding, check the current text and report: fixed / partially fixed / still
  open / regressed. Ch 6 was excluded from the 2026-06-10 pass — give it a full first pass.
- Build check: `cd /home/kacper/thesis/thesis/latex && latexmk -pdf thesis.tex` (or its
  Makefile). Confirm it builds clean with no undefined/duplicate refs or overfull-box disasters.

## Method (non-negotiable)
1. Do NOT trust the prose. Every factual or quantitative claim — every number, ratio, "X×",
   "novel", "first", "unbiased", "matches the reference", "faster", "less memory" — must be
   traced to evidence in FINDINGS / the code / a cited paper / the reference impl. If you can't
   find support, that is a finding. If the evidence contradicts the prose, that is a finding.
2. Read the actual code before asserting what the renderer or the Mitsuba baseline does. Do not
   hallucinate behavior from names.
3. Cross-chapter consistency: the same quantity/claim must not differ between abstract, Ch 5,
   Ch 6, Ch 7, conclusion. Hunt contradictions.
4. Distinguish "the claim is wrong" from "the claim is unsupported by the included evidence" —
   both matter, label which.

## Review dimensions (operationalized — map findings to these)
- **Truthfulness / no overclaiming.** Unsupported or inflated numbers; "novel/first" without
  attribution; results stated more strongly than the data licenses; throttled/uncontrolled
  measurements presented as clean.
- **Important-detail disclosure (defense landmines).** Are limitations, biases, residual
  errors, and caveats stated where the claim is made — not buried? Examples to actively probe:
  any residual validation error, scene-dependence of a "win," clock/throttling caveats on
  timings, sample-count and convergence criteria, what was NOT validated. A hidden caveat an
  examiner finds is worse than a disclosed one.
- **Fairness of comparison.** Is the Mitsuba baseline configured fairly (equal quality target,
  same hardware, same scene/sampler semantics — NEE vs. analog, equal SPP-or-equal-time,
  identical clocks)? Self-serving baseline configuration is a classic attack — verify the
  protocol against the code/scripts, not the description.
- **Experimental methodology / reproducibility.** Sample counts, variance/error bars or noise
  estimates, number of views/scenes, hardware + driver + clock disclosure, ablation rigor,
  whether "measured" things were measured or asserted. Could a reader reproduce it?
- **Attribution / novelty calibration.** Argmin/ADT scatter sampler was proposed by Condor;
  product-RIS is a ReSTIR/Talbot-2005 adaptation. Confirm credit is correct and the
  contribution is framed as synthesis/realization/measurement, not invention, where that's true.
- **Academic rigor.** Definitions before use, notation consistency (no symbol collisions),
  citations point to the published venue not just arXiv, theorem/claim soundness, equations
  correct.
- **Structure & narrative.** Does it read as a thesis with a clear thesis statement and through-
  line, not a stitched lab log? Is scope honest? Is the contribution legible to an examiner in
  the first two pages? Are negative results framed as contributions, not apologies?
- **Relevance / comprehensiveness.** Anything load-bearing missing (e.g., a claimed result with
  no supporting figure/table); anything present that distracts from the contract.

## Severity rubric (tag every finding)
- **CRITICAL** — an examiner with the artifacts would call it overclaiming, a contradiction, or
  a wrong result. Blocks submission.
- **MAJOR** — factually wrong, unfair, or misleading; fix before submission.
- **MINOR** — imprecise, incomplete, or under-supported; should fix.
- **NIT** — polish (typo, phrasing, citation format).

## Calibration (be fair, not performative)
- Do not invent problems. If a claim checks out against the evidence, do not flag it; if you
  verified something nontrivial and it holds, say so briefly.
- Include a short "Strengths / do not touch" list so the author knows what is already solid.
- Prefer fewer, higher-confidence findings over a long speculative list. Mark genuine
  uncertainty as uncertainty; never dress a guess as a verified defect.
- Suggest the minimal honest fix (reword, qualify, run-the-missing-measurement, or cut), not a
  rewrite into a different thesis.

## Output
Write a single markdown review to
`/home/kacper/thesis/thesis/reviews/<today>-full-review.md`. Do NOT edit the thesis itself.
Structure:
1. One-paragraph verdict: is it submission-ready against the contract above? Top risks.
2. "Act-first" list: the CRITICAL/MAJOR items in priority order, one line each.
3. Status of prior-review findings (fixed / open / regressed).
4. Per-chapter findings. Each: `[SEVERITY] file:line — problem — evidence (what in
   code/FINDINGS/paper supports or contradicts it) — concrete fix.`
5. Cross-cutting passes (notation, citations, tense/consistency).
6. Strengths / do-not-touch.
Anchor every finding to a `file:line` (or PDF page) and to the evidence you checked.
