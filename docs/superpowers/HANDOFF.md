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
8. **`--ris` CLI registration bug (CODE).** `config.cpp` registers only `--denoise`; `--ris` /
   `--ris-candidates` are plumbed (`config.h:45`, `renderer.cpp:207`) but never registered, so RIS is
   unreachable at runtime and "shipped behind a runtime flag" (Ch 6, and Ch 4 §4.6 implication) is false
   in the current binary. Fix the lost registration in `config.cpp`; then Ch 6 + Ch 4 Implementation note
   can truthfully call RIS a runtime flag.
9. **MIS description in Ch 6:93–94** — repeats the (now-fixed in Ch 4) one-shadow-ray error; the code does
   **two** shadow rays (phase-IS + env-IS balance-MIS, `constants.cuh:152`). Mirror the corrected Ch 4 §4.5.
10. **Appendix A contradictions** (fix during the A1→Ch 6 fold): premise inversion vs FINDINGS §8.27
    ("original §8.5 premise was RIGHT"); the track-length hybrid is **closed** (§8.32, "dead end … three
    ways"), not "open" — scope any residual openness to the flat-env regime; soften the depth-1-row claim.
11. **§8 numbers wording** — `~5×` deficit is `~5.5×` in FINDINGS §8.5 (Ch 8 + Ch 6 round the same; tighten
    to `~5.5×` if desired).

## Non-Ch 6 deferred TODOs from the 2026-06-10 review (thesis-wide, low-priority polish)

The CRITICAL/MAJOR review items are FIXED (commit after this handoff). These remain:

- **Tense sweep** (review §9.3): CONVENTIONS reserves past tense for measured results; Ch 5 uses a
  timeless-property voice ("the renderer matches", "passes the furnace"). Decide the rule (the abstract
  was already moved to past tense) then sweep, or amend CONVENTIONS to bless the timeless voice.
- **Notation: MC sample count `N`** (review §9.1) — `02-background.tex:20` and `05-validation.tex:65`
  use `N` for spp, colliding with `N` = primitive count. Switch sample-count `N` → `spp`. (The mixture
  `K`→`N` and `K_k`→`C_k` collisions are already fixed.)
- **`k` vs `k²` convention** (`05-validation.tex:65`): thesis `k = RMSE²·N` = FINDINGS' `k²`; note the
  convention when campaign numbers land so a transcribed `kC=0.411` isn't squared-wrong.
- **Remaining bib upgrades** (review §9.4): MipNeRF/PixelNeRF still arXiv `@misc` (cited once each);
  canonical BVH cites (Rubin&Whitted / Kay&Kajiya / Goldsmith&Salmon) vs `MacDonaldBooth1990` +
  the lecture-slides `SpatialAccelerationStructures`; EWA splatting (Zwicker 2001) if EWA is reintroduced;
  Woodcock proceedings title; DOI spot-check (handoff item 11). DSYG/3DGS/NeRF already upgraded.
- **Minor prose nits not yet done** (review §2–§4): `02-background.tex` BVH cite + `p` triple-duty
  symbol; `04-architecture.tex` `:91` "fast local memory" wording, `:73-74`/`fig:pipeline` `N`→`H`,
  "$O(N+A)$ absorbs H". Low priority.

---

# EXPERIMENTS HANDOFF (2026-06-11) — next Claude: continue the campaign

**Read first:** `docs/superpowers/specs/2026-06-10-section6-experiments.md` (the revised runbook —
§0 preconditions, G1–G8, methodology, and the appendix with the exact A/B command recipe), then
`results/campaign/*.md` (each banked experiment's record). This section is the experiments-side
continuity; the thesis-writing handoff above still applies.

## Banked (clock-independent → FINAL, no full-blast redo needed)

| Result | Record | Commit |
|---|---|---|
| Per-asset cap table (4-asset lineup + WDAS spread) | `scripts/tools/caps_table.csv`, `tab:overlap` | `fd7479c`,`7933951` |
| AS/IAS memory cloud+bunny (compaction demoted to S-mode) | `results/campaign/gas_memory.csv` | `3b65474`,`7aac675` |
| RIS unbiasedness on the meadow (G3 correctness gate) | spec §G3 note | `db119e3` |
| ncu profile + real roofline fig (our megakernel; latency-bound, 6.95/32 lanes) | `results/campaign/ncu_summary.md`, `roofline.csv`, `figures/roofline.pdf` | `ee11ad4`,`7badf7f` |
| Mitsuba JIT/startup (#96 measurement; ours 0.39 s vs prb 0.85/2.28 s) | `results/campaign/jit_overhead.md` | `3c2b4a9` |
| Active-set cap sensitivity nil (64↔128); Ch 4 sentence added | `results/campaign/caps_ab.md` | `7933951` |
| Split overflow counters + positive "Cap check" log | code | `480f812` |
| Per-asset caps VERIFIED necessary+sufficient (tornado 112/432, explosion 32/176, bunny 320/496; furnace re-gates; scattering = binding stress) — §0.1–0.3 DONE | `results/campaign/caps_per_asset.md` | `f09811d` |
| G8 icosphere (branch `feature/icosphere-gas`): compile-guarded port; accuracy on 3 assets (optimum N=2, universal N=3 sliver reversal); perf REFUTES hypothesis — analytic pays 1.17–1.58× (RT-core triangles vs software sphere IS); scattering gate 0.073 %; DSYG/Mitsuba shell lookups resolved; Ch 6 `sec:icosphere` + `tab:icosphere`, (I) row emptied, Ch 4 corrected | `results/campaign/icosphere_port.md`, `icosphere.csv` | `c61fbbd`…`9c6e791` |
| G2 RR-depth sweep (window): shallow 8–12 efficiency basin (min 10); dev-era "11 %" → +3.4 % ± 2 % (Ch 6 softened, real `fig:rr-depth`); k FINAL, absolutes provisional → optional timing-only rerun reuses EXRs | `results/campaign/rr_depth.{md,csv}`, EXRs `rr_seeds/` (gitignored) | `799c3f2` + |
| G3 K-sweep MEADOW rung (window, contention-free → absolutes final): RIS 1.481× [1.467,1.490], K=4–6 plateau tied → K=6 default survives; −21 % time + −14 % variance; MIS-arm k replicates RR sweep to 4 decimals; Ch 6 `sec:ris` updated. Flat + studio rungs REMAIN (studio needs `SG_ENV` wiring) | `results/campaign/ris_ksweep.md`, `ris_ksweep_meadow.csv`, EXRs `ris_seeds_meadow/` | this commit |

## Remaining — runnable NOW (no full-blast window)

1. **Peakiness script** — small committed script computing max/mean + energy-top-0.1% for the three
   envs (flat 1× / studio ~700× / meadow ~2×10⁵) so `fig:ris-ksweep`'s x-axis is reproducible. The
   RGBE decoder to reuse is inline in this conversation's history and trivial to rewrite (~40 lines).
2. **G8 icosphere A/B — accuracy + GAS-size axes** (perf axis needs the window). Needs the §0.5 code
   port from `eb5372f` (`Icosphere<N>` in `include/thesis/host/geometry/mesh.h`, `TriangleGAS` in
   `gas.h`). **Kacper greenlit the concept; confirm before sinking the day.**
3. **Mitsuba-parity gates** for bunny/tornado/explosion (spec §3): converged-mean energy-ratio method
   (§8.25 template); fix the asset-camera vertical flip vs Mitsuba; constant-env first. Gates all
   cross-renderer numbers for the new assets.
4. **Per-asset cap recompiles**: tornado **112/432**, explosion **32/176**, bunny **320/496** —
   each + a furnace/1-seed re-gate (spec §0.3). The new "Cap check" log line certifies each.
5. **Validation-ladder montages** (Ch 5 figs) — assemble from renders (ours + Mitsuba), clock-indep.
6. **Implementation plan** for the window (runner scripts: seed sweeps, inter-seed-noise/k extraction,
   CSV emitters per spec §6). Spec is approved; superpowers `writing-plans` is the next step.

## Window-only (needs Prybicki to lift the 150 W cap + lock clocks)

G1 headline (incl. the **flat-env rung** — the ~5.5× deficit number), G2 merge-ladder ablations +
RR sweep {5..16}, G3 K-sweep ×3 envs, G4 bunny re-profile (after 320/496), G6 wavefront
(within-`feature/wavefront-phase1` A/B) + adaptive (converged-ref RMSE), G8 perf axis.

## Hard-won gotchas (do not relearn these)

- **GPU state poisons timing:** prybicki's desktop session made a day ~3× slower per spp; a naive
  before/after showed a phantom 2× regression. Interleave any A/B; never compare across sessions;
  all reported timings from the locked-clock window. (`caps_ab.md` tells the story.)
- **Asset scenes IGNORE `--width/--height`** (scene-native 900×600 for `cloud_asset_*`). Env/camera
  via env vars: `SG_ENV=meadow`, `SG_CAM=0`, `SG_PLY=<path>` (asset_validation; default bunny).
- **`test_runner` HAS `--ris`/`--ris-candidates`** (plan-review act-3 checked the standalone app, which
  lacks them — only fix if that binary ships). Adaptive is still compile-time.
- **Multi-build A/B technique:** `OPTIXIR_PATH` is baked absolute → swap `build/device_program.optixir`
  + exe pairs in place (recipe in the spec appendix). Canonical repo state = 128/128.
- **ncu:** SASS FLOP counters return 0 on OptiX kernels — use pipe counters (`sm__inst_executed_pipe_fma`)
  × avg-active-lanes; ncu base-clocks itself (fine, ratios); profile recipe in `ncu_summary.md`.
- **Mitsuba runs** via `tools/refs/with_jorge_mitsuba.sh`; cold-cache by `env HOME=<tmpdir>`;
  `tools/refs/jit_overhead_timing.py` takes `INTEGRATOR=prb|tomography`.
- **exr_diff:** `tools/refs/.venv/bin/python tools/refs/exr_diff.py a.exr b.exr` (means + signed-mean).
- Commit per result, NO AI mentions; `git push`/branch deletion are Kacper's. `deprecated-*` branches
  hold every old experiment's code (incl. the orphan-recovered ones — do not `git gc`-prune carelessly).

## Open decisions for Kacper

G8 port greenlight (timing) · explosion's no-emission look (confirm with Jorge) · R3 `stress_N` sweep
and Mitsuba-side peak VRAM: add or rescope Ch 7 · the Ch 6 rework queue (above) still pending his pass.
