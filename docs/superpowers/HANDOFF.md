# Handoff — Thesis writing (for the next Claude)

**Read first:** this doc · `docs/superpowers/specs/2026-06-08-thesis-design.md` (the design spec) ·
`docs/superpowers/plans/2026-06-08-thesis-execution.md` (the execution plan) ·
`thesis/latex/CONVENTIONS.md` (voice + notation). Then `git log --oneline` on branch `thesis`.

## Status (2026-06-08)

- **Branch `thesis`.** Build: `cd thesis/latex && latexmk -pdf thesis.tex` → expect **0 errors**, ~38 pp.
- **Written + committed:** Ch 2 Background, Ch 3 Related Work, Ch 4 Architecture (flagship), bibliography,
  `CONVENTIONS.md`. Ch 1 is still the *old* draft intro (to be retargeted). Ch 5 Validation, Ch 6 Optimization, and Ch 8 Conclusion are also written + committed; only Ch 7 Results and Appendix A remain stubs.
- **Remaining:** **Ch 7 Results** (figure/experiment-heavy — needs the campaign), **Appendix A** (the A1 write-up), **Ch 1** retarget, **abstract**.
- **Spine source:** `thesis/FINDINGS.md` §0–§8.38 (the lab notebook). Spec §6 maps FINDINGS §-entries → chapters.
- **For deep code detail** (Ch 4 used this): run an **Explore agent** over `device/ include/ src/` rather than
  guessing — don't re-derive the architecture from memory.

## How Kacper wants you to work (explicit asks — honour these)

- **Preserve-and-elevate, do NOT replace wholesale.** The original draft is his work. Before cutting any of
  his prose, **show BEFORE/AFTER** (`git show b61e5cc:thesis/latex/chapters/<file>` = original) and let him
  decide. He reviewed Ch 2/3 cuts item-by-item; do the same for anything substantive.
- **Voice:** third-person, present tense, terse; foreshadow the contribution; past tense only for measured
  results. Per `CONVENTIONS.md`. He approved it — keep it. (It runs *tight*: chapters land ~half the spec's
  page budget; that's fine, figures + the autopsy ledger carry pages. See the page projection in the spec.)
- **Commit per chapter; NO AI/tool mentions in commit messages or anywhere.** `git push` and
  `git branch -d` are **USER** actions — never do them. `git reset --hard` is blocked.
- **Compile after every change** (`latexmk`); zero `!` errors before committing.

## THE TWO CAP / OVERFLOW ISSUES (Kacper raised these — address them)

### A. Build a per-asset cap-estimation script — NOT yet written

`MAX_ACTIVE_PRIMS` and `HIT_BUFFER_CAPACITY` (`device/core/constants.cuh`, both **128**) are **compile-time**
and **asset-dependent** (sized for the cloud, whose overlap ≈ 45). A denser/different asset can exceed them.
Kacper wants an **offline** script that estimates the right cap **before** recompiling for a new asset.

Design (no GPU, no recompile needed to estimate):
- Standalone Python; reuse the existing PLY loader (`tools/refs/npy_asset_to_ply.py` / the converter) →
  per-primitive **centre, scale, quaternion** → its **3σ ellipsoid**.
- **`MAX_ACTIVE_PRIMS`** ← max number of primitives whose 3σ ellipsoid simultaneously **contains a point**
  (max point-overlap).
- **`HIT_BUFFER_CAPACITY`** ← max number of primitive **entries a single ray crosses** (ray–ellipsoid hits).
- Use the **same criterion as the renderer's active set** (`point_inside_ellipsoid`, 3σ) so the estimate
  matches the code — not a looser bounding box.
- Sample points/rays through the scene AABB (or the actual target camera rays); report **max + p99 +
  histogram**; suggest `cap = max × margin`, rounded up.
- Properties: **offline** (decouples cap-sizing from the build — answers "render a different asset, what
  then?" → run estimator → set cap → recompile), and **slightly over-estimates** (3σ geometric bound ≥
  density-significant overlap → **safe**). Bonus: running it on all 4 assets gives the **per-asset overlap
  table** that substantiates the thesis cap claim.
- Suggested location: `scripts/tools/estimate_caps.py` (or under `tools/refs/`).

### B. "Overflow handling" — frame it correctly (Ch 4 already states the accurate version; keep consistent)

It means **safe + detectable, NOT correct**:
- Hit buffer full → any-hit **drops the excess hit, keeps traversing** (bounds-checked, no crash).
- Active set full → `CompactSet` insert **returns `false`** (excess primitive not added).
- Both increment a device atomic **`overflow_counter_`** (`device/core/launch_params.cuh`,
  `include/thesis/common/params/launch_params.h`) → host detects overflow after the render.
- **Consequence:** dropped prims/hits → **under-absorption → image too bright**. A biased-but-graceful,
  **detected** failure — a *fix* over the prior **silent**-drop bug (FINDINGS §6 / closed task #63).
- Ch 4 §4.6 already says the accurate thing (caps above the **cloud's recorded ~45** + a collinear stress
  test + the runtime overflow counter; **not** "measured worst-case across all assets"). **Once script (A)
  is built and run,** upgrade the claim to "estimated per-asset by `estimate_caps.py`, confirmed by the
  overflow counter (0 overflows)" and add the per-asset overlap table to Ch 6/7.

## Open items (Kacper input or follow-up)

- **Ch 4 §4.4 novelty-credit line** — `% TODO(confirm…)` in `04-architecture.tex`. Confirm with Kacper what
  **Jorge proposed vs. what Kacper realised**, and whether *"first realisation for this representation"* is the
  claim to commit to at the viva.
- **Ch 4 enrichment** (Kacper leaning yes): architecture/pipeline **diagram**, **complexity before/after
  table** (`optimizations.md` Big-O: per-bounce O(N+A²)→O(N+A), sort O(H²)→0, bisection→0), and an **argmin
  pseudocode** block (`algpseudocode` is loaded).
- **Task #96:** quantify Mitsuba **JIT/startup overhead** (Ch 7) + expand the Mitsuba-limitations paragraph (Ch 3).
- **Experiment campaign** (spec §9, full-blast at the 3090's full clock via the admin "Prybicki" — NOT the
  150 W cap) is **not yet run**; it produces Ch 5–7 figures. Write the prose now; generate figures from
  existing FINDINGS numbers where possible and leave clearly-marked placeholders otherwise. Kacper's
  prerequisites (spec §13): opt-inventory code pass, schedule the Prybicki window — not blocking Ch 5 prose.

## Key locations

- LaTeX: `thesis/latex/` (USI `memoir`; `siunitx` + `cleveref` loaded). Figures: `scripts/plots/figure_from_csv.py`
  via `tools/refs/.venv`. Background math source: `references/pbr-book` (PBRT, gitignored symlink).
- Caps/overflow code: `device/core/constants.cuh`, `device/core/launch_params.cuh`,
  `include/thesis/device/utils/{bit_vector,compact_set}.h`.
- Original draft (for BEFORE/AFTER): `git show b61e5cc:thesis/latex/chapters/0{1,2,3}-*.tex`.

## STILL TODO — full gap analysis (2026-06-08)

Ordered by how load-bearing they are.

1. **Experiment campaign + ALL data figures — the big unlock.** No experiments run yet, so every plot
   is absent. Ch 5 and Ch 6 carry marked `% FIGURE (campaign): …` slots (validation ladders, furnace,
   RIS K-sweep, money-shot, GAS-compaction/memory) but no figures. Gates Ch 7 entirely. Needs the
   full-blast GPU window (admin Piotr Rybicki) per spec §9.
2. **Ch 7 Results** — empty stub; depends on (1).
3. **Ch 1 Introduction** — still the OLD draft intro (future tense, no contributions list / structure);
   retarget to the finished work.
4. **Abstract** — still `TODO`.
5. **Appendix A (A1)** — empty stub; write up the A1 investigation (FINDINGS App A).
6. **Ch 4 §4.4 novelty-credit line** — `% TODO(confirm…)`: what Jorge proposed vs. what Kacper realised.
7. **Per-asset cap-estimation script** (see section A above) — not written.
8. **Mitsuba JIT/startup overhead** (task #96) — not measured; small Ch 7 comparison.
9. **Ch 4 enrichment** — architecture/pipeline diagram + argmin pseudocode still missing (a complexity
   table already exists, in Ch 6 `tab:complexity`).
10. **Front matter:** acknowledgements DONE (Piotr **Rybicki** — friend/machine owner — + family);
    title-page date still says "Lugano, September 2025" (stale, update); **advisors Didyk & Jorge are NOT
    yet acknowledged** — confirm with Kacper whether to add them, and whether the "low-level graphics
    mentorship" credit belongs to Rybicki (as written) or to an advisor.
11. **Bibliography:** spot-check the DOIs deliberately omitted on some classics.
12. **Final polish (Phase 3):** cross-chapter consistency pass (notation σ_t/τ/erf, tense, terminology);
    confirm every reported number is at the single full-blast operating point.

**Solid / done:** prose backbone Ch 2–6 + Ch 8 drafted, cited, compiling clean (~49pp); spec, plan,
CONVENTIONS, bibliography (39 refs), figure pipeline.

## Ch 6 rework — queued items from the 2026-06-10 review (do in the Ch 6 pass)

Ch 6 is Kacper's WIP; these were decided during review but deferred so the Ch 6 rework integrates them coherently:

1. **Fold Appendix A (A1) into Ch 6** (Kacper decision). Move `chapters/appendix-a-a1.tex` content into
   `sec:autopsies` as a full subsection (expand the A1 bullet). Then delete `appendix-a-a1.tex`, remove
   `\appendix` + `\input{chapters/appendix-a-a1}` from `thesis.tex`, and repoint `\Cref{app:a1}` (used in
   Ch 6) to the new subsection label. Rationale: consistency — wavefront/adaptive are Ch 6 bullets, so A1
   should not get a lone appendix.
2. **`tab:complexity` "Boundary sort (scatter)" row** — reword; the reference does NOT sort (verified: it
   marches + per-segment running-min selection + bisection). Change to "boundary marching/selection" to
   stay consistent with the abstract/§4.4/Fig 4.1 corrections (commit 38c3d25).
3. **§6.1 "Optimisation began with profiling, not guessing"** — remove (Kacper: false + pretentious;
   FINDINGS §8.5 was explicitly "never profiled" at first).
4. **§6.2 four-modes table overflows the page** (Kacper #24) — resize/restructure (e.g. shorten the
   Examples column or use a smaller font / `\resizebox`).
5. **Roofline figure** (Kacper #28) — placeholder already in `build_figures.sh` + `figures/roofline.pdf`;
   still TODO: a dedicated roofline plotter (log-log with memory/compute roofs from the ncu numbers) and
   wiring `fig:roofline` into `sec:bottleneck`.
6. **Overflow counter — code question** (Kacper #27, deferred). Decide whether to keep the device
   `overflow_counter_` at all now that `estimate_caps.py` sizes caps offline. Keep: cheap runtime
   assertion that the (sampled, conservative) estimate held, esp. for an un-estimated new asset. Drop:
   if caps are always estimated first, it is belt-and-suspenders. Code decision, not prose.
7. **Feature inclusion/exclusion is Ch 6's job** — §5.5 was trimmed (bug-confession cut) and now hands the
   "which features to enable, at what cost" narrative to Ch 6. Make sure the perf study delivers it.
