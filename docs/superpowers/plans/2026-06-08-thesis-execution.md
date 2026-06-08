# Thesis Execution Plan

> **For agentic workers:** this is a **writing + experiments** plan, not a code-TDD plan. Tasks are
> chapter-sections / experiment-groups / infra items, each with **Files · Deliverable · Acceptance · Commit ·
> Depends-on**. Steps use checkbox (`- [ ]`) syntax. The **spec** (`docs/superpowers/specs/2026-06-08-thesis-design.md`)
> holds the *content* (WHAT each chapter argues); this plan sequences it (HOW / WHEN / order).

**Goal:** Produce the ~80pp master thesis (LaTeX, in-repo) + its figures, sequenced to an **end-of-June draft** to
Piotr → review → **28 Aug defense**.

**Approach:** *Reuse-and-elevate.* Restructure the 31pp draft + FINDINGS §8 into the 8-chapter spec skeleton; new
writing concentrates in **Ch 4** (the novel N1+N2 architecture) and **Ch 7**. Re-measure all reported numbers at full
blast and turn them into figures via a matplotlib pipeline. Experiment-independent chapters are written in parallel
with the experiment campaign.

**Tech stack:** LaTeX (USI `memoir`); the CUDA/OptiX renderer (`test_runner`); Mitsuba / `volprim_prb`
(`tools/refs/.venv`); Python + matplotlib for figures; PBRT (`references/pbr-book`) for Background math.

**Working pattern fit:** daily evening writing blocks; experiments scheduled via Claude remote control (so the
experiment campaign is **pipelined**, kicked off early, and consumed by the writing as results land).

---

## File structure (LaTeX working copy)

Extract the zip's `thesis/...` tree into **`thesis/latex/`** and split the monolithic `01-introduction.tex` by chapter:

```
thesis/latex/
  thesis.tex                 # main; \input each chapter; title/author/advisors already set
  abstract.tex               # rewrite (currently TODO)
  chapters/
    01-introduction.tex      # split from the monolith
    02-background.tex
    03-related-work.tex
    04-architecture.tex      # NEW — flagship (N1+N2)
    05-validation.tex
    06-optimization.tex
    07-results.tex
    08-conclusion.tex
    appendix-a-a1.tex        # the A1 full investigation (FINDINGS App A)
  refs.bib                   # extend + verify (spec §11)
  figures/                   # generated thesis figures (committed)
  images/                    # existing draft images (bvh-dragon, gsplats-pipeline, …)
  {extrapackages,layoutsetup,theoremsetup,macrosetup}.tex, Makefile, USIlogo.*
scripts/
  plots/                     # CSV → publication figure (matplotlib)
  experiments/               # batch harness driving test_runner at full blast
results/                     # experiment CSVs / exrs (gitignored if large)
```

One responsibility per chapter file → each is small enough to hold in context while writing/editing.

---

## Phase 0 — Infrastructure & prerequisites  *(Day 0–1)*

### Task 0.1: Extract & restructure the LaTeX working copy

**Files:**
- Create: `thesis/latex/**` (from `thesis/Efficient_..._Volumes.zip`)
- Create: `thesis/latex/chapters/0{1..8}-*.tex`, `thesis/latex/chapters/appendix-a-a1.tex`
- Modify: `thesis/latex/thesis.tex` (replace the single `\input{thesis/01-introduction}` with per-chapter `\input{chapters/...}`)

- [ ] **Step 1** — Extract the zip into `thesis/latex/` (flatten the inner `thesis/` level so paths are `thesis/latex/thesis.tex`, `thesis/latex/images/...`). Fix `\includegraphics` paths (`thesis/images/...` → `images/...`).
- [ ] **Step 2** — Split `01-introduction.tex` at its `\chapter{}` boundaries into `chapters/01..03` (Intro, Related Work, Background as they currently are); create empty stubs `chapters/04..08` + `appendix-a-a1.tex` each with just its `\chapter{...}` line.
- [ ] **Step 3** — Rewrite `thesis.tex`'s body to `\input` all eight chapters in order + the appendix; reorder so **Background precedes Related Work** (per spec §6).
- [ ] **Acceptance:** `cd thesis/latex && make` (or `latexmk -pdf thesis.tex`) produces `thesis.pdf` with the new chapter scaffold and no missing-file/citation *errors* (undefined refs OK for now).
- [ ] **Commit:** `git add thesis/latex && git commit -m "thesis: in-repo LaTeX working copy, split into per-chapter files"`

**Depends-on:** none.

### Task 0.2: Figure / plot pipeline

**Files:** Create `scripts/plots/style.mplstyle`, `scripts/plots/figure_from_csv.py`, `thesis/latex/figures/.gitkeep`

- [ ] **Step 1** — Add a shared matplotlib style (consistent fonts/sizes matching the `memoir` body; vector PDF output).
- [ ] **Step 2** — Write `figure_from_csv.py` (run via `tools/refs/.venv/bin/python`): reads a results CSV + a small spec (x/y/series/labels) → writes a `figures/<name>.pdf`.
- [ ] **Acceptance:** a smoke-test (`figure_from_csv.py` on a 3-row dummy CSV) emits a valid PDF that `\includegraphics` renders in a test build.
- [ ] **Commit:** `git commit -am "thesis: matplotlib figure pipeline (CSV → publication PDF)"`

**Depends-on:** none. *(Use `tools/refs/.venv` — system python lacks numpy/scipy/matplotlib.)*

### Task 0.3: Full-blast experiment batch harness

**Files:** Create `scripts/experiments/run_fullblast.sh`, `scripts/experiments/runlist.md`

- [ ] **Step 1** — Enumerate the **reported-number set** (the V/O/R runs from spec §9) as a concrete table in `runlist.md`: scene, flags (`--ris`, `--rr-depth`, `--spp`, `--sigma-multiplier`, `--hg-g`, …), seeds, output CSV path.
- [ ] **Step 2** — Write `run_fullblast.sh`: iterates the runlist, invokes `build/bin/Release/test_runner` (or the project's binary) with each config, writes CSVs to `results/`. A `--dry-run` prints the matrix without running.
- [ ] **Acceptance:** `run_fullblast.sh --dry-run` prints the full reported-number matrix; one real smoke run produces a parseable CSV.
- [ ] **Commit:** `git commit -am "thesis: full-blast experiment batch harness + runlist"`

**Depends-on:** 0.5 (opt inventory) for the O-set rows; the run can start before the full-blast *window*.

### Task 0.4: Bibliography

**Files:** Modify `thesis/latex/refs.bib`

- [ ] **Step 1** — Add the technique→citation set from spec §11 (Kajiya'86, Veach&Guibas'95, Arvo&Kirk'90, Chandrasekhar'60, Henyey-Greenstein'41, Woodcock'65, **Kutz'17/SDTracking**, **Talbot'05/RIS**, Chao'82/Vitter'85, Bitterli'20/ReSTIR, Sobol'67/Owen'98, MacDonald&Booth'90, NVIDIA SER'22, **PBRT 4e'23**, WDAS cloud'17, Turk&Levoy'94, **Stochastic Splats**).
- [ ] **Step 2** — **Verify each entry's author/year/venue** (WebSearch / the papers in `papers/`) — wrong years are viva-bait.
- [ ] **Acceptance:** `bibtex`/`biber` runs clean; a scratch `\cite` of each new key resolves.
- [ ] **Commit:** `git commit -am "thesis: bibliography — verified canonical citations for techniques used"`

**Depends-on:** none.

### Task 0.5: [USER] prerequisites (parallel, not blocking writing)

- [ ] **0.5a** — Code-completeness pass on the §8 optimization inventory (spec §8): flag anything missed/removed so the four-mode triage is complete and current.
- [ ] **0.5b** — Confirm the three open items (spec §13.3): N1/N2 novelty boundary; the two papers' roles; the inverse-CDF formulation (`−log(1−χ)` vs `χ`).
- [ ] **0.5c** — Schedule the **full-blast window(s)** with Prybicki for Phase 1.

---

## Phase 1 — Experiment campaign  *(full-blast, pipelined; Days 1–10)*

> Kick off early via remote control; results feed Ch 5–7. Each task = run → CSV → sanity-check. All at full clock,
> equal-quality (noise²·time) where timing; furnace as the bias gate; Mitsuba runs same operating point.

### Task 1.1: Validation set (V)
- [ ] Re-run / confirm figure-ready validation numbers: furnace (§8.1), absorption ladder (§2–7), scattering ladder vs Mitsuba-analog (§8.2–8.4), features (§8.6–8.14), money shot (§8.11).
- [ ] **Acceptance:** each matches FINDINGS within noise; CSVs + reference exrs under `results/validation/`.

### Task 1.2: Optimization set (O) — hero cloud, + memory where relevant
- [ ] RR-depth sweep (§8.33) · RIS-vs-MIS + K-sweep, **env vs flat** (§8.37) · adaptive on/off (§8.30) · denoiser spp-curve (§8.22) · fast_erf (§8.21) · `ncu` bottleneck profile (§8.28/8.29) · **GAS compaction before/after (memory)** · **wavefront RayState footprint (memory)**.
- [ ] **Acceptance:** ablation *ratios* consistent with FINDINGS at full clock; CSVs under `results/optim/`; ncu report saved.

### Task 1.3: Results set (R1–R7) — the ✦ new compute
- [ ] R1 perf vs Mitsuba (equal-quality), hero + 1 breadth spot-check · R2 ⭐ **4-asset generalization** (visual + RMSE vs Mitsuba/volprim_prb) · R3 scaling (`stress_N` + 4 assets; time & memory vs N) · R4 showcase finals (renders) · R5 **firefly 3-way** (ours / Mitsuba-analog / Mitsuba-MIS; max, p99.9/p99.99, firefly-pixel count, crops) · R6 footprint breakdown (Disney cloud) · R7 vs-Mitsuba peak VRAM.
- [ ] **Acceptance:** per-experiment sanity (numbers plausible, renders clean); CSVs/exrs under `results/results/`.

### Task 1.4: Generate all figures
- [ ] Run `scripts/plots/figure_from_csv.py` over every results CSV → `thesis/latex/figures/*.pdf`; assemble render-comparison montages.
- [ ] **Acceptance:** every figure referenced in Ch 5–7 exists and renders in a build.
- [ ] **Commit:** `git commit -m "thesis: experiment results + generated figures (full-blast)"`

**Depends-on:** 0.2, 0.3, 0.5c.

---

## Phase 2 — Writing  *(Days 2–18; chapters in dependency order)*

> **Order rationale:** experiment-*independent* chapters (4, 2, 3) start immediately, in parallel with Phase 1;
> experiment-*dependent* chapters (5, 6, 7) follow as figures land; framing chapters (1, 8, abstract) last.
> Each task: write the chapter per its spec section; **Acceptance** = compiles clean, covers the spec's bullets,
> citations + figure refs resolve, tense is past/active; **Commit** per chapter (or per major section).

### Task 2.1: Ch 4 — Architecture & Implementation  *(FLAGSHIP; start first)*
- [ ] **Files:** `thesis/latex/chapters/04-architecture.tex`. **Content:** spec §2.1 (N1 single-trace anyhit-buffer + analytic exits; N2 ADT argmin) + §6 Ch 4 row. **Sources:** code (`device/`, `src/`), `abstract_kindof.txt` (N1 prose), `optimizations.md` Big-O tables, CLAUDE.md.
- [ ] Write the **flagship novelty section**: N1 (anyhit-only + backface-cull → entries, analytic exits, 1-vs-N traversals) and N2 (ADT free-flight argmin, SDTracking Theorem 1, analytic-erf inversion), with **prior-art positioning** vs DSYG/Mitsuba marching + Stochastic Splats, and the **honest-synthesis framing**.
- [ ] Write the supporting architecture: analytic erf optical depth, GAS/IAS instanced spheres, the data structures (BitVector/CompactSet dispatch, `HitBufferSoA`), megakernel design, RR, transforms, camera-inside.
- [ ] **Acceptance:** covers N1+N2+prior-art+erf+GAS/IAS+megakernel; compiles; complexity Big-O table included; ≥1 architecture diagram referenced. **Commit.**
- [ ] **Depends-on:** 0.1; confirm 0.5b (inv_cdf formulation) before finalizing the N2 math.

### Task 2.2: Ch 2 — Background  *(start early)*
- [ ] **Files:** `chapters/02-background.tex`. **Content:** spec §6/§7 — keep MC/GMM/rendering-equation; **add** (PBRT-sourced) the volume rendering equation, transmittance, **free-flight sampling**, **phase functions (HG)**, **NEE + MIS**, DSYG **analytic erf optical-depth**. Fix the Epanechnikov formula; resolve the SfM TODO.
- [ ] **Acceptance:** every concept Ch 4/5/6 rely on is defined here; compiles; cites PBRT/Veach/HG. **Commit.**
- [ ] **Depends-on:** 0.1, 0.4.

### Task 2.3: Ch 3 — Related Work  *(start early)*
- [ ] **Files:** `chapters/03-related-work.tex`. **Content:** spec §7 cut/keep — tighten NeRF/Mitsuba/VDB/3DGS/DSYG to engineering-thesis altitude; **cut** BVH-construction mechanics, BSP/kd taxonomy, rasterization deep-dive, generic application lists, the duplicate Mitsuba paragraph; **ADD the novelty lineage:** SDTracking / decomposition tracking + Stochastic Splats (sort-free).
- [ ] **Acceptance:** positions the contribution against DSYG + SDTracking + stoch-splats; compiles. **Commit.**
- [ ] **Depends-on:** 0.1, 0.4.

### Task 2.4: Ch 5 — Validation Methodology & Correctness
- [ ] **Files:** `chapters/05-validation.tex`. **Content:** spec §6 — elevate FINDINGS §0–§8.14; **lead with the differential-validation pivot narrative** (voxel-grid references unreproducible → "run Mitsuba, quantify"); the equal-quality / firefly methodology (§0, §8.0); the absorption + scattering ladders; features; the money shot; **frame the ladder as the unbiasedness proof of N2/ADT.**
- [ ] **Acceptance:** the pivot is told; every claim cites a FINDINGS § + a V-figure; compiles. **Commit.**
- [ ] **Depends-on:** 0.1, 0.4, 1.1, 1.4.

### Task 2.5: Ch 6 — Performance Engineering & Optimization
- [ ] **Files:** `chapters/06-optimization.tex`. **Content:** spec §8/§9 — the starting gap (§8.5) → profiling/bottleneck (§8.28/8.29) → the **four-mode** presentation (M wins + RIS highlight; C complexity-argued w/ Big-O; S standard-practice w/ the one-line justifications; I icosphere) → the **autopsy ledger** (each: hypothesis→kill-test→measurement→why→lesson; the megakernel-shaped structural lesson) → the **memory** sub-section (GAS compaction, the wavefront RayState plot).
- [ ] **Acceptance:** every opt placed in a mode; autopsies are first-class; the §0.5a final inventory is reflected; figures resolve; compiles. **Commit.**
- [ ] **Depends-on:** 0.1, 0.5a, 1.2, 1.4.

### Task 2.6: Ch 7 — Results
- [ ] **Files:** `chapters/07-results.tex`. **Content:** spec §9 R-set — R1 final perf vs Mitsuba; R2 generalization (the 4 assets, the "it generalizes" figure); R3 scaling (time & memory vs N); R4 showcase; R5 firefly 3-way; R6 footprint; R7 vs-Mitsuba VRAM. All full-blast.
- [ ] **Acceptance:** each result has a figure/table + prose; the headline speedup + firefly + memory differentiators are stated with numbers; compiles. **Commit.**
- [ ] **Depends-on:** 0.1, 1.3, 1.4.

### Task 2.7: Ch 1 — Introduction
- [ ] **Files:** `chapters/01-introduction.tex`. **Content:** spec §1–§4 — retarget the existing intro from future→past tense; state the problem, the contribution boundary (Jorge physics / Kacper systems), the contribution list (N1+N2 architecture, validation methodology, optimization study incl. RIS), the thesis structure. Mine §3 context (the motivation; learning OptiX from zero).
- [ ] **Acceptance:** contributions match §2; reads as completed work; compiles. **Commit.**
- [ ] **Depends-on:** 2.1–2.6 substantially drafted (so the contribution claims are accurate).

### Task 2.8: Ch 8 — Conclusion & Future Work + Appendix A
- [ ] **Files:** `chapters/08-conclusion.tex`, `chapters/appendix-a-a1.tex`. **Content:** summary of contributions; limitations (FINDINGS §9 — emission, Gabor kernels, animation, SER Ada-only, the caps); future work (the original extensions: emissive/SGGX/parametric/fluids; SER on Ada; inverse rendering via SLANG.D). Appendix A = the A1 full investigation (FINDINGS App A).
- [ ] **Acceptance:** limitations honest; future work ties to the original proposal; compiles. **Commit.**
- [ ] **Depends-on:** 2.1–2.7.

### Task 2.9: Abstract
- [ ] **Files:** `thesis/latex/abstract.tex`. **Content:** rewrite from `abstract_kindof.txt` to the *current* state — foreground the N1+N2 novel architecture, the differential validation, the optimization study + RIS, the full-blast results (beats Mitsuba; firefly-free; compact memory).
- [ ] **Acceptance:** ≤1 page; matches the contribution statement; compiles. **Commit.**
- [ ] **Depends-on:** 2.1–2.8.

---

## Phase 3 — Polish & submit  *(Days 18–21)*

### Task 3.1: Full build & cross-refs
- [ ] Resolve all undefined refs/citations/figure-refs; eliminate LaTeX warnings; check every `\ref`/`\cite` resolves.
- [ ] **Acceptance:** clean `latexmk` build, zero undefined references. **Commit.**

### Task 3.2: Consistency & narrative pass
- [ ] Tense (past/active), notation (σ_t, τ, erf conventions), terminology (ADT, kernel-mixture), and that **both through-lines** (differential validation; negative-results) are visible. Read end-to-end for flow.
- [ ] **Acceptance:** a coherent read; no notation clashes. **Commit.**

### Task 3.3: Length/balance + front matter
- [ ] Trim/balance toward ~80pp (no chapter starved/bloated); acknowledgements (one line on the personal context if desired), declaration of originality, title-page date.
- [ ] **Acceptance:** ~80pp; front matter complete. **Commit.**

### Task 3.4: Submit
- [ ] Send the PDF to Piotr by **end of June**. (Then: July review iteration → upload → 28 Aug defense.)

---

## Self-Review — spec coverage

| Spec section | Covered by |
|---|---|
| §1 what it is / §2 contributions / §4 framing | T2.1 (arch/novelty), T2.7 (intro), T2.9 (abstract) |
| §3 context / motivation | T2.7 (intro), T3.3 (acknowledgements) |
| §5 constraints (full-blast, deadline) | Phase 1 (full-blast), Phase 3 (deadline), 0.5c |
| §6 skeleton (8 chapters) | T0.1 scaffold + T2.1–2.9 |
| §7 front-half cut/keep/expand | T2.1–2.3 |
| §8 four-mode triage (+ inventory) | T2.5 + 0.5a |
| §9 experiment plan (3 axes, run list) | Phase 1 (1.1–1.4) |
| §10 autopsy write-up | T2.5 (ledger) |
| §11 bibliography | T0.4 |
| §12 logistics (PBRT, plot pipeline) | T0.1, T0.2 (PBRT already symlinked) |
| §13 open threads | 0.5a/b/c |

**Gaps:** none structural. The only *content* dependencies are the user prerequisites (0.5a inventory, 0.5b
confirms, 0.5c window) — flagged, non-blocking for the experiment-independent chapters.

**Note on granularity:** writing tasks are deliberately chapter/section-sized (not 2–5-min code steps) — prose can't
be pre-written in the plan; each task instead carries its content-source (spec §), acceptance, and commit. The
genuinely mechanical, step-able tasks (0.1–0.4) use finer steps.
