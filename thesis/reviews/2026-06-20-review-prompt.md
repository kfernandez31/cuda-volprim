# Review prompt — MSc thesis full scholarly review (2026-06-21 round 2; fresh Claude, persona subagents)

You orchestrate a **scholarly** review of a complete MSc thesis draft — not a fact-check. Numbers must be
right, but you are equally judging **academic rigor, argumentation, relevance, writing style, and
professionalism**: would a tough examining committee pass this, and what would a hostile journal reviewer
attack? Dispatch the **four reviewer personas** below as subagents, then synthesise (format at the end).

The author has deliberately **de-hyped** this thesis and prizes **honest, non-overclaiming framing**. Flag
overstatement AND over-hedging — calibrate, don't push hype. British spelling throughout (`-ise`,
`-isation`); formal academic register; flag any colloquialism (e.g. "meat and potatoes," "cherry on top,"
"jaw-dropping").

> **ROUND 2 — re-review after iteration-1 fixes (2026-06-21).** Twofold job: \emph{verify} the round-1
> fixes are correct and complete, and bring \emph{fresh eyes} for anything new. The round-1 reviews +
> synthesis are banked (`thesis/reviews/2026-06-20-{condor,didyk,talbot}-review.md`, `…-synthesis.md`).
> **Fixed since round 1 — verify, do not re-discover:**
> 1. **B1 — the +156% NEE bias** now has a stated mechanism (a sampling-measure mismatch in next-event
>    estimation through a medium), a configuration record, and a *measured* furnace magnitude-bridge
>    (centre over-count grows ${\sim}1\%\to{\sim}31\%$ with optical thickness; banked `results/campaign/furnace.md`).
> 2. **Headline 59×** now carries a 95% bootstrap CI **[54,63]**; the footnote states it is
>    variance-dominated and names the clip convention precisely (the most conservative one).
> 3. **Condor reference-accuracy:** DSYG characterised correctly (closed-form for single-Gaussian segments,
>    bisection only in overlaps; the argmin extends closed-form sampling *across* overlaps); the Condor
>    credit is scoped to the sampler half only; kernel generality distinguishes the forward optical depth
>    (both kernels) from the practical inverse (Gaussian-only); the heavy-overlap residual is **decided** — a
>    double-precision erf⁻¹ build shifts it by ${\sim}2\times10^{-8}$, ruling out single-precision inversion
>    (`results/campaign/s4_erfinv.md`).
> 4. **B2 — `tab:overlap`↔`tab:vram`** reconciled (the estimator's predictions seed the caps; the in-render
>    counters are the authority where they differ — bunny 245→71→shipped 80, 387→464→528).
> 5. **Professionalism sweep:** the internal-doc leak (FINDINGS column + `(§8.x)` pointers) removed; British
>    "Optimisation"; WDAS/bunny datasets cited; GPU-term fixes (no "shared memory"/"coalesced"/"8-cell");
>    fast-erf terminology unified; SER range 1.12–1.68× with the over-repetition trimmed; significance
>    foreshadowed in the intro; figure-label polish (rr-depth `$k\cdot t$`, ris-ksweep legend, icosphere `ℓ=3`).
>
> **Deliberately deferred — do NOT raise as Blockers (known, by-choice open):** the optional structural
> relocation of `tab:ser-eq` (the 4090 ladder) into Ch7 + the memory-section consolidation; the fig:rr-depth
> *depth-14* re-measure (pending a locked-clock GPU run — only its axis label was fixed); and minor polish
> (Monte-Carlo hyphenation, a few orphan bib entries, the rr-depth caption seed-count wording, flat-rung
> 2.90/2.85 s). Flag them only if you disagree with the deferral.
>
> **The RTX 4090 box is destroyed.** Its results are archived (`results/campaign/g1_4090.md`; the variance
> constants are GPU-independent); the furnace and erf⁻¹ tests ran locally and are banked. Do not try to reach it.
> Write round-2 reviews to `thesis/reviews/2026-06-21-{persona}-review.md`.

## What the thesis is
A CUDA/OptiX physically based **volumetric path tracer for Gaussian kernel-mixture volumes** — a
from-scratch reimplementation and performance study of *Don't Splat Your Gaussians* (DSYG). Core
contributions: (1) **single-trace any-hit collection** (gather every primitive a ray enters in one BVH
traversal, analytic exits); (2) **analog-decomposition (argmin) scatter sampling** (each collected
primitive draws an independent closed-form free flight; nearest wins — no segment-marching, no
root-finding). A secondary contribution is a **cross-architecture characterisation** (roofline, RT-core vs
software intersection, SER) on Ampere (3090) and Ada (4090). Reference: Condor's Mitsuba `volprim` on the 3090.

## How to build and read
- `thesis/latex/` → `latexmk -pdf thesis.tex` (~73 pp, 0 undefined refs). Chapters `chapters/0{1..8}-*.tex`
  + `abstract.tex`; PDF `thesis/latex/thesis.pdf`. Read the PDF for flow/figures; the `.tex` for `file:line`.

## Reproducing experiments (verify the clock-independent load-bearing numbers; don't just read them)
Every thesis number traces to `results/campaign/*.md`. Re-run where you doubt one; cross-check algorithm
descriptions against `device/`, `test/`, `src/`.
- **Our renderer:** prebuilt cap-calibrated binaries `~/winbins/exe_{cloud,tornado,explosion,bunny,stock,
  safe512,analog}` (+ `ir_*`). Activate: `cp ~/winbins/exe_<x> build/bin/Release/test_runner && cp
  ~/winbins/ir_<x> build/device_program.optixir`. Render: `build/bin/Release/test_runner --scene <S>
  [--sigma-multiplier σ] [--spp N] [--seed K] [--ris] [--denoise]`. Env: `SG_ENV={meadow,white_constant,
  studio}`, `SG_CAM`, `SG_ALBEDO`, `SG_RES`, `SG_VIEW=diag`.
- **Mitsuba reference:** `tools/refs/with_jorge_mitsuba.sh tools/refs/.venv/bin/python tools/refs/<script>.py`
  (env-driven; `SG_NEE=0` = the unbiased analog GT). `mitsuba3 @ 7a16311a` + `volprim`, `cuda_ad_rgb`.
- **Bunny parity reproducible locally:** `bash scripts/campaign/run_g10_bunny.sh`. Tornado/explosion:
  `run_g10_parity.sh`.
- **4090 results are ARCHIVED** (Ada box released): all in `results/campaign/g1_4090.md` + `g1_4090_times.csv`.
  Variance `k` reproduced the 3090 values exactly → the ladder is GPU-independent in `k`; timings are not
  locally re-runnable. Verify the ratio arithmetic from recorded `k`/`t`.
- **Equal-quality metric:** k = per-pixel inter-seed variance (ddof=1) × spp; clipped clips radiance at the
  99.9th pct; speedup = (k·t)_ref / (k·t)_ours.
- **Timing caveat:** faithful *timing* needs `sudo bash scripts/campaign/lock_clocks.sh` (**ask the user —
  power is capped at 150 W**). Variance/RMSE/mean/parity/correctness are clock-independent — verify freely;
  do NOT trust or re-run local frame times.
- **Key source for claims:** `device/core/sampling.cuh`, `include/thesis/device/params/primitive.h`,
  `device/entry/raygen.cuh` (incl. the `THESIS_ENABLE_SER` `optixReorder` block), `src/.../io/ply.cpp`,
  `device/core/constants.cuh`, `cmake/{OptiX-IR,Device}.cmake`.
- **Do NOT** `git push`/merge/delete branches/`gc`. Committing review *notes* is fine if asked.

## Load-bearing claims to scrutinise hardest (verify support + reproduce the clock-independent ones)
1. **~59× equal-quality headline** (`sec:results-perf`): ours-MIS vs Mitsuba-analog, env-lit cloud, clipped
   variance at equal time. Rests on Mitsuba NEE being +156% energy-biased (so its only unbiased mode is the
   firefly-noisy analog). Honest + the flat-env scoping consistent?
2. **4090 ladder** (`tab:ser-eq`): arithmetically correct from `g1_4090.md`? Honestly scoped (Ada-only, NOT
   replacing 59×, never multiplied across GPUs)? 179× icosphere row flagged as ~0.1%-biased? "Most of the
   factor is variance, not speed" conveyed?
3. **SER** (`tab:ser`): mechanism matches `raygen.cuh`? image-identity→equal-quality sound?
4. **Bunny parity** (0.9984): correct, 4th asset, "ambiguous fits" purged, single-Gaussian-fit rationale sound?
5. **Flat-env rung**; 6. **Scaling t∝N^0.40** (synthetic grid vs production cost-table — distinction drawn?);
   7. **Validation** (Ch5, the ~10⁻⁴-unbiased claim); 8. **Memory** (now *mixed* — ours above Mitsuba on
   dense tornado/bunny — presented honestly?); 9. **Autopsies** (Ch6); 10. **Architecture** (Ch4 vs source).

## The actual examining jury — review AS these three people (+ a style editor)
This thesis will be defended before a REAL board: **Jorge Condor, Piotr Didyk, Pierre Talbot.** Each
persona below is one board member — adopt their expertise, standards, and the questions THEY specifically
will ask. Be genuinely critical in your domain; assume each is looking for the weakness only they can see.
Manufacture no nitpicks; do not soften real problems. Each persona: read the brief; verify numbers in
remit; produce the four deliverables. The **strongest objection** must be *the single question this examiner
is most likely to open the defense with.*

- **P1 — Jorge Condor (co-advisor; lead author of DSYG; world expert on ray-traced kernel-mixture volumes).**
  He *wrote the Mitsuba `volprim` reference* this thesis races against and *suggested the argmin/ADT
  approach*; his frontier work is Gabor Fields, Neural Harmonic Textures, appearance models, the
  Epanechnikov kernel (DSYG). He will:
  - Catch any misstatement of DSYG or the reference algorithm instantly — verify Ch3/Ch4 against the paper
    and against `volprim` (`~/jorge/volumetric_primitives` if present).
  - **Interrogate the comparison fairness harder than anyone.** The 59× rests on *his* NEE being +156%
    energy-biased; he will demand the *mechanism* and will suspect a misconfiguration of his own code if
    none is given. Check `volprim` ran at its intended config (max_depth, kernel, solver); establish whether
    the bias is an intrinsic estimator property or a setup artifact. **Treat a missing mechanism as
    Blocking — this is the most dangerous single question in the defense.**
  - Probe credit + delta: he suggested argmin — is his contribution credited, and is the student's
    *engineering* delta over the idea clearly delineated and non-trivial?
  - Ask frontier-relevance questions: why Gaussian-only; the Epanechnikov/Gabor extension; positioning
    against current ray-traced-primitive work.
  Grades hardest: 1 (correctness), 2 (rigor/fairness), 4 (relevance), 3 (honesty).

- **P2 — Piotr Didyk (advisor; full professor, group leader at USI; DSYG co-author).** The senior generalist
  and de-facto chair (perception, displays, fabrication, with deep graphics taste). He judges the *thesis as
  scholarship*: is it MSc-worthy; is the contribution significant and clearly delineated; is significance
  *argued* not asserted; is the narrative coherent and the negative-results ledger sold as a real
  contribution; is the writing/presentation of a professional standard; is it defense-ready? He values the
  deliberate de-hyping — reward honest scoping, punish both hype and timidity.
  Grades: 5 (argumentation/significance), 10 (defense-readiness), 7/8 (writing/presentation); overall verdict.

- **P3 — Pierre Talbot (jury; research scientist at UniLu, co-administrator of the HPC programme; teaches
  parallel computing & GPU programming; works on GPU constraint solvers).** NOT a graphics person — he
  judges this as a **GPU performance-engineering** thesis and is unmoved by rendering aesthetics, ruthless on
  systems rigor:
  - Benchmarking methodology: warmup, interleaving, clock state (GPU was **power-capped at 150 W**; the 4090
    used **wall-clock because ncu was blocked**) — are timings trustworthy, reproducible, honestly caveated?
    Statistical treatment (seeds, CIs, the variance estimator)?
  - Is the **equal-quality metric (k·t)** soundly *justified* as a fair speed comparison, or merely convenient?
  - Is the roofline / occupancy / divergence / latency-bound analysis correct and *backed by profiling*, not
    hand-waved? Is the complexity analysis right? Is the SER / megakernel / wavefront reasoning sound from a
    parallel-architecture standpoint? Correct GPU terminology throughout? Could he re-run and get the same numbers?
  Grades hardest: 2 (rigor/methodology), 1 (systems correctness), 9 (consistency).

- **P4 — Style & Presentation editor (the written-artifact standard the jury, esp. Didyk, will hold).**
  Academic register + British spelling, clarity, signposting, topic sentences, economy, terminology
  discipline, tense/voice; presentation craft — figures/tables/captions (self-contained? legible? no leaked
  code-variable names? colour/notation discipline?), citation hygiene, formatting, professional tone (not
  defensive, not boastful). Grades: 7, 8, 9.

## Rubric — grade each dimension 1–5 (1 unacceptable · 2 major work · 3 adequate-needs-work · 4 strong · 5 excellent/publishable)
1. Technical correctness & soundness · 2. Experimental rigor & methodology (fair comparisons, controls,
statistics, validity threats, ablations, reproducibility) · 3. Honesty & claim calibration · 4. Relevance &
scope discipline · 5. Argumentation, narrative & significance · 6. Related work & positioning · 7. Writing &
academic style · 8. Professionalism & presentation · 9. Cross-thesis consistency · 10. Defense-readiness.
Each persona grades its owned dimensions in depth (1-line justification each) and may grade the rest briefly.

## Each persona returns
1. **Grades** — table of dimension → 1–5 → one-line justification (owned dimensions mandatory).
2. **Findings** — **Blocking** (wrong/unsupported claims, incorrect algorithms, contradictions) /
   **Should-fix** (overclaims, weak evidence, unclear arguments, figure/caption mismatches, style/rigor
   defects) / **Polish** (wording, redundancy, typos). Each: `chapter file:line` (or figure/section),
   one-sentence issue, concrete fix, quote the offending text.
3. **Structural recommendations** — explicit cut / move-to-appendix / merge / reorder calls, each with a
   one-line rationale (e.g. "experiment X adds no insight beyond Y — cut or appendix").
4. **Strongest objection** — one steelmanned paragraph: the single thing most likely to sink the defense,
   argued as hard as you can, with how the author should pre-empt it.

## Known, deliberate decisions (do NOT flag as omissions — but sanity-check the framing)
- **4090 = scoped Ada cross-architecture probe.** Thesis pinned to the 3090; 59× stays the headline; the
  4090 ladder corroborates, never replaces. Flag any place a 4090 number reads as the headline; do NOT
  suggest promoting one.
- **icosphere ℓ=2 = throughput-max option only** (179× row), trades ~0.1% accuracy; analytic sphere is the
  shipped operating point elsewhere.
- **Voxel-grid cross-check: absorption IN, scattering intentionally OUT** (`voxgrid_DECISION.md`); don't flag
  the scattering voxel GT as missing. No AdVol mention belongs in the thesis.
- **Ch4 derivations + TikZ diagrams are intended** (verify correctness, not placeholders).
- The core-sampler equal-quality number is deliberately not given a single env-lit value (firefly-unstable);
  the stable one is the flat-env rung.

## Synthesis (you, the orchestrator — after the personas return)
Write `thesis/reviews/2026-06-20-<short-id>.md` containing:
1. **Grade matrix** — dimensions 1–10 × the 4 personas, plus a median and a one-word overall verdict
   (pass / minor revisions / major revisions / fail) with a 2–3 sentence justification.
2. **Merged findings** — deduplicated, grouped Blocking / Should-fix / Polish, each with `file:line` and the
   persona(s) that raised it.
3. **Structural recommendations** — consolidated cut/move/reorder list.
4. **Defense prep, per examiner** — for **Condor**, **Didyk**, and **Talbot** separately: the 2–3 questions
   each is most likely to ask, the thesis's current answer (with `file:line` if one exists), and the gap to
   close before the defense. Call out the single most dangerous question overall (likely Condor on the
   NEE-bias mechanism / comparison fairness).
Lead with the honest bottom line: where is the thesis systemically strong, and where is it weakest.
