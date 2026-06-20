# Section 6 experiment-plan review (2026-06-10)

**Object:** `docs/superpowers/specs/2026-06-10-section6-experiments.md` (G1–G7 lineup).
**Question:** do the experiments make sense — rigor, showcase value, right quantities. Feasibility secondary.
**Verified against:** `thesis/FINDINGS.md` (§8), code on `thesis` branch (`device/`, `src/`, `include/`, `scripts/`),
chapters 04/05/06/07/08 + abstract (post-`95fa7bf`), `scripts/plots/` + `results/campaign/`,
`scripts/tools/caps_table.csv`, git branches, and `reviews/2026-06-10-full-review-ex-ch6.md`. Spec claims
were checked against artifacts, not trusted.

**Severity:** CRITICAL = the set cannot support the claim it exists to produce, or a group is
unrunnable/ill-defined as designed. MAJOR = a number produced as planned would be invalid or misleading.
MINOR = fix in the implementation plan.

---

## 0. Act first

1. **[CRITICAL] Add a flat-env cross-renderer rung (bare, final, Mitsuba-analog on the constant-env cloud).**
   The headline G1 exists to support "closed an initial ~5× equal-quality deficit" — a *flat-lighting*
   number (§8.5: 1.93× per-spp × 2.85× noise; abstract now says "under flat lighting" explicitly). G1/G2
   run only meadow configs, where there never was an equal-quality deficit (§8.11/§8.15: pre-optimisation
   CUDA already beat Mitsuba-analog ~630× on cost there, firefly-driven). As designed, the run cannot
   produce its own headline number. →§1-G1.
2. **[CRITICAL] Re-found G2's ablation protocol.** "Leave-one-out from the all-on configuration" is not
   executable on the current binary and is ill-defined for entangled wins: none of the six M-mode wins is
   a runtime toggle; the shadow-transmittance "before" code was deleted (incl. `sorting.cuh`, §8.16), and
   the any-hit fusion presupposes that rewrite, so removing §8.16 from final forcibly removes §8.18 too.
   Decide: historical merge-commit ladder (re-run each win's own A/B pair at locked clocks) + true
   leave-one-out only for real toggles. →§1-G2.
3. **[CRITICAL, code precondition] G3 is unrunnable: `--ris` / `--ris-candidates` are not registered in the
   CLI** (`config.cpp` registers neither; fields exist at `config.h:45-46`; full review act-first #3).
   K was never CLI-exposed at all, so the K-sweep needs new plumbing in the app *and* the test-runner
   config path, then the G3 furnace rung doubles as the regression gate. Make this sequencing step 0
   alongside the caps recompile. →§4.
4. **[MAJOR] Drop "equal-quality" framing vs Mitsuba-MIS.** On the showcase Mitsuba-MIS converges to a
   +155% wrong image (§8.11; +6.5% furnace §8.1). Matching its noise constant means "reaches the wrong
   answer faster." Report vs MIS: per-spp time, firefly stats, and its bias; confine equal-quality ratios
   to ours-vs-analog. →§2.
5. **[MAJOR] Take the denoiser AND adaptive sampling out of the k protocol.** Both are biased and (adaptive)
   variable-N; inter-seed variance cannot see denoiser bias or adaptive's −4e-4 early-stop bias, and
   k = noise²·N is ill-defined when N varies per pixel. Use the §8.22/§8.30 protocol: RMSE vs a converged
   reference, labelled "effective". →§2.
6. **[MAJOR] Freeze the "final" config per group.** fast-erf is a CMake opt-in, default OFF (§8.21) yet
   listed as a kept win; RIS is default-off; clamp/Gaussian-filter/denoiser are opt-in. Unpinned, the
   headline, ablations, and RIS study can silently run different "finals". Define final-validation
   (exact erf, box filter, no clamp, no denoiser, MIS) vs final-showcase, and state which feeds each
   figure; every k-measured config must be the unbiased one. →§1-G1/G2.
7. **[MAJOR] Gate the bunny with a converged-mean check before any cross-renderer number.**
   Scattering-mean agreement exists for the cloud only; the asset-side Mitsuba script supports constant
   env only and the `asset_validation` camera was found vertically flipped vs Mitsuba (§8.25 caveats).
   →§1-G1.
8. **[MAJOR] G4: the nsys "trace/scatter/escape/shading wall-clock split" is unobtainable** — the bounce
   loop is one megakernel (Ch 4); nsys resolves kernel-level only. Drop it or plan an instrumented
   (`clock64()`) build and present it as such. Frame the roofline strictly as a non-saturation argument.
   →§1-G4.
9. **[MAJOR] Reconcile the plan with Ch 7's R-set.** R2 (4-asset generalisation incl. smoke/embergen),
   R3 (time/memory vs N scaling, `stress_N`), and R7's Mitsuba-side peak VRAM have no experiments in this
   plan. Rescope Ch 7 now or add the cells; silent under-delivery resurfaces at writing time. →§3.

---

## 1. Sense — per group

**Overall:** every group feeds a named thesis claim; the re-run-vs-cite triage of negatives (G6) is the
right cut; no group is busywork (G7 is the lowest-value — one number for a limitations footnote — but
costs minutes). The holes are structural, not conceptual: the flat-env rung (act-1), the ablation
protocol (act-2), and metric validity at the edges (act-4/5).

**G1 (headline).** Configs are meadow-only; the deficit-closure claim is flat-env-defined (act-1).
Concretely add to G1/G2: constant-env cloud × {bare-baseline, final, Mitsuba-analog}, 16 seeds, plus
Mitsuba per-spp time. Cheap cells (no fireflies → k stabilises fast) and they simultaneously support
Ch 6's currently unqualified "the equal-quality comparison against Mitsuba inverts"
(`06-optimization.tex:57-59`) — which without a flat measurement at final remains the §8.16/§8.23
*extrapolation* ("~6× per-spp vs analog" was explicitly extrapolated, never re-benchmarked). Also pin
which binary "this renderer (final)" is (act-6): RIS on or off on the meadow changes the headline by
~1.4×. Bunny rung: act-7 — without a mean-agreement gate (the §8.25 energy-ratio method, 0.9999 on
wdas8, is the template) "equal quality vs Mitsuba" on bunny is not defensible; fallback is scoping bunny
to ours-internal scaling. Firefly reporting (clipped+unclipped, max/percentiles) is the right design;
prefer p99.9/p99.99 + max over max alone (max is seed-unstable).

**G2 (ablations).** Act-2 is the core problem. Reality of the six "toggles" on `main`: shadow-ray
transmittance — old path deleted; skip-containment-scan — merged, no toggle; dedup-bounce-0 — merged, no
toggle; any-hit fusion — merged, no toggle, and entangled with §8.16; fast erf — CMake flag (default
OFF); denoiser — `--denoise` (the only true runtime toggle). Reverting wins one-at-a-time on top of
final means re-implementing N variants of the renderer and re-validating each — and for §8.16/§8.18 the
"one out" subtraction is not even well-defined. The merge-commit ladder (checkout each win's own
merge/parent pair, re-run at locked clocks) reproduces exactly the A/B that produced `tab:wins`, with
zero new code; its numbers are "sequential at the historical point", which is what `tab:wins` already
cites. If marginal-at-final numbers are wanted for some wins, do leave-one-out only where a real toggle
exists (denoiser, fast-erf, RIS, RR via `--rr-depth`). Whichever is chosen, label the semantics in
`tab:wins` — sequential and marginal-at-final differ (the shadow rewrite was ~85%-of-frame when measured;
its marginal at final is a different number).
Bare→final endpoints (G2a) are sound once the bare SHA is pinned (pre-§8.16 validated state; RR at its
historical default 5 — the 5→12 retune is part of the cumulative story).
RR sweep: include depth 5 (the old default the thesis cites, `06:57`, §8.33); {4,6,8,10,12,16} brackets
it but omits the anchor of the "5→12" claim.
Fast-erf bias gate (converged mean, fast vs exact): correct, and shares machinery with act-5's
converged-reference protocol.

**G3 (RIS ladder).** Sound, and the strongest methodological upgrade over dev-time: a measured middle
point (studio) turns the two-point flat/meadow split into a trend claim; K=1 is the built-in
consistency anchor (= plain env-IS NEE, §8.37); the equal-quality ratio is *within-env* (RIS vs MIS under
the same map), so the three HDRIs need no energy matching; the furnace re-confirm is the right gate and
doubles as the post-CLI-fix regression gate (act-3). Dev K-sweep measured {2,4,8,16} with K=6
interpolated — the planned {1,2,4,6,8,12} closes that gap; if the full-blast peak lands at 4 rather
than 6, `06:111` "peaking near K=6" still reads, but let the data set the default.

**G4 (profiling).** The load-bearing numbers for `sec:bottleneck` (occupancy ~22%, eligible-warps,
long_scoreboard-dominant stalls, SOL SM%/DRAM%, 114 regs, 82–98% cache residency) are all ncu — keep.
256² grid-fill choice verified (§8.28). Two corrections: act-8 (nsys can't split a megakernel; the only
honest nsys content is kernel-level: render vs denoise vs I/O); and the roofline is usable *only* as a
non-saturation visual (SM 34.5% / DRAM 1.1% ⇒ the point sits far under both roofs ⇒ latency-bound) —
caption it that way or readers will read "under the memory roof" as bandwidth-bound/badly-optimised,
the exact misreading `sec:bottleneck` refutes; note GFLOP/s undercounts the SFU/erf-heavy mix. Also:
no `fig:roofline` float exists in any chapter and no plotter exists — this is new tex + a new plotter,
not "already wired". ncu locks clocks to base by default (`--clock-control`); its metrics are ratios so
that's fine, but record it so "full-blast" isn't claimed for ncu rows.

**G5 (memory).** Fine: GAS before/after is logged and parseable (`acceleration_structure.h:64-66`);
per-ray state numbers are ncu-derivable; NanoVDB "cite if unavailable" is honest. Add the free check:
log the runtime overflow counter per run (it exists, `renderer.cpp:357-366`) — zero-overflow on cloud
@128 and bunny @new-caps directly evidences the Ch 4 "detected, zero on validated scenes" claim at the
operating point.

**G6 (negatives).** Right triage: wavefront and adaptive carry quantitative weight in Ch 6; the rest are
cited. Wavefront: A/B *within* `feature/wavefront-phase1` (THESIS_WAVEFRONT ON vs OFF at the same
commit), not branch-vs-main, or the ratio absorbs post-fork changes; one full-blast confirmation point
+ the cited dev range (100–1400×) is the right shape. Adaptive: act-5 — its dev verdict (~2× slower at
equal quality) came from RMSE-vs-converged-GT; the noise-only k protocol would hide its bias and
mis-rank it. Note adaptive's runtime flags did not land on `main` (`constants.cuh:187` constexpr; no
`--adaptive-*` in `config.cpp`) — rebuild-toggle or restore the flags.

**G7 (JIT).** Fine as a one-shot fairness number. State explicitly that steady-state per-frame numbers
elsewhere exclude startup on *both* sides (ours: OptiX-IR load; Mitsuba: JIT) so the cost isn't
double-counted in any k·t.

**Load-bearing and missing:** the flat rung (act-1), the bunny gate (act-7), R3-or-rescope (act-9),
overflow logging (free, above). Pointless experiments: none.

---

## 2. Methodology specifics

**Reference-free equal-quality (16 seeds, k = noise²·N).** Sound for the intended case: both estimators
unbiased (ours: furnace + ladder; Mitsuba-analog: furnace-exact §8.1), so noise *is* error, and k is the
right spp-invariant constant — matches Ch 5's definition (`05:65`; use the thesis k-convention, not
FINDINGS' k², when transcribing). Three qualifications:
- **Applies only to unbiased, fixed-N outputs.** Excluded: Mitsuba-MIS means (act-4), denoiser, adaptive,
  firefly clamp (act-5/6). For those, converged-reference RMSE (one 2048-spp uniform GT per scene — the
  §8.30 machinery) and "effective" labels.
- **Heavy tails break finite-N invariance.** Mitsuba-analog-meadow's k is spp-dependent (fireflies
  converge slower than 1/√N — FINDINGS already says k·t *understates* our edge). Pin and report the
  measurement spp; lean on clipped-k + percentiles for tail-heavy configs (the spec already reports
  both — make the spp pinning explicit).
- **Uncertainty.** 16 seeds pooled over ~10⁶ pixels is ample for k, and the 16 runs give timing
  spreads for free — but report seed-bootstrap CIs on every equal-quality *ratio*, and for the ≤3%
  effects (fusion, fast-erf) keep the interleaved-A/B ordering even at locked clocks (locking removes
  thermal drift, not all run-to-run jitter; §8.35 saw ~9% jitter swamp a small effect).

**Leave-one-out:** see act-2 — the protocol, not the idea, is the problem.

**Peakiness ladder:** sound (see §1-G3). The peakiness numbers (700×, 2×10⁵×, energy-in-top-0.1%) should
be produced by a committed script so the figure's x-axis is reproducible, not quoted from a one-off.

**Bias gates:** the right set — furnace (reference-free, scatter-side) re-confirmed under RIS; absorption
analytic gates exist as dev artifacts; fast-erf via converged means. Add: *any* binary change in step 0
(CLI restoration, caps recompile) re-gates via furnace + a 1-seed cloud diff — cheap, and it converts
"same binary we validated" from assumption to evidence.

---

## 3. Framing — experiments vs thesis claims

- **The flat-lighting scoping is now load-bearing.** Post-review abstract: "closed an initial roughly
  fivefold equal-quality deficit **under flat lighting** and left the renderer ahead … on the
  environment-lit showcase." Act-1 is what makes that sentence supportable from this run; it equally
  serves `06:4-7` (intro gap statement, unqualified — Ch 6 rework should inherit the qualifier) and
  `08:23` (same claim, unqualified).
- **Mitsuba-MIS:** the thesis frames it as "the realistic perf/firefly competitor" — that framing
  survives only as per-spp time + firefly statistics + stated bias (act-4). Any table row labelled
  "equal quality" vs MIS on the showcase is indefensible at a defense.
- **`tab:wins` semantics** must match the act-2 decision (sequential-historical vs marginal-at-final);
  currently the table cites the sequential dev measurements, so the merge-ladder reproduces like for
  like.
- **"Every reported number comes from this run"** needs two explicit carve-outs the spec half-makes:
  (a) cited dev-time negatives (stated); (b) the Ch 5 ladder/montage *images and RMSE numbers* —
  correctness artifacts are clock-independent, so reusing dev renders is legitimate, but say so, and
  either schedule the float64 brute-force analytic rungs that `fig:absorption-ladder`'s caption promises
  for pair/cloud (full review M3) or fix the caption first. No experiment group currently renders the
  ladders.
- **RIS:** the ladder directly evidences the corrected, scene-dependent claim (1.4× env / loses flat),
  and G3's flat rung is the in-run measurement of the "~2.5× worse" half. Keep the §8.37 honesty note
  (adaptation of Talbot/Bitterli, not novelty) — Ch 6 already does.
- **No over-reach found** in G4–G7 relative to chapter claims, *given* act-8's reframing of the
  roofline/nsys items.

---

## 4. Feasibility (secondary — showstoppers and wiring only)

- **[CRITICAL] act-3:** `--ris`/`--ris-candidates` absent from the CLI (G3 blocked); adaptive runtime
  flags absent on `main` (G6-adaptive needs rebuild or flag restoration). Renderer-code preconditions,
  not runner-script work — schedule in step 0 with the furnace regate.
- **Build inventory implied by the plan** (record build+flags per CSV row; never mix across assets):
  cloud 128/128 build; bunny build — the estimator says **320/496** (`caps_table.csv`), not "≥320/≥560";
  560 is the 24 576-prim WDAS row. Cite 496 or state the margin policy ("estimator-sized" currently
  misquotes the estimator; oversizing matters on sparse assets per Ch 4, less on dense per §8.25).
  Plus: pinned bare-baseline SHA; wavefront branch pair; fast-erf CMake variant; per-act-2 ablation
  builds.
- **Pipeline wiring:** `rr_depth.csv` schema drift (spec: `…,noise,k`; committed CSV/README: `…,rmse`)
  and `build_figures.sh` plots **frame_ms vs depth** — monotone rising, no knee, contradicting
  `fig:rr-depth`'s "at matched quality" caption. Plot efficiency (k·t). `ris_ksweep.csv`/`gas_memory.csv`
  match. Roofline: new plotter + new figure float needed (§1-G4). `wins.csv`/`headline.csv` are new as
  stated.
- **Tooling that does not exist yet** (expected — implementation-plan scope): seed-sweep runner,
  EXR→inter-seed-noise/k computation, converged-GT RMSE harness reuse. Renderer outputs suffice
  (EXR-only; frame time and GAS sizes parseable from logs).
- **Budget:** plausible but tight. The second long pole after the 16-seed bulk is Mitsuba-analog on the
  meadow (high spp × 16 seeds for tail-stable clipped-k; bunny-analog adds more). The added flat-env
  cells (act-1) are cheap. The compression valve (drop bunny from per-win ablations) is the right one.
- **Run-log pinning (one line each):** render resolution per scene (Ch 5 names none; dev used 800²
  cloud / 512² assets — "the Chapter 5 validation resolution" is currently undefined), Mitsuba
  version/variant, env-rotation convention (90° about +Y, §8.6), seed lists both sides, locked clocks,
  driver/CUDA/OptiX versions (spec already lists the last), "studio optional in G1" → decide now.
