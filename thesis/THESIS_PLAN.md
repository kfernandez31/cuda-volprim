# Thesis Completion Plan

**Status (2026-06-05):** Renderer validated ≤1e-4 vs Mitsuba across the ladder; decisively
faster (~6× per-spp vs Mitsuba-analog, ~32× equal-quality vs its biased MIS — *extrapolated,
re-benchmark pending*); denoiser mode working. Core validate+optimize campaign essentially done.
Remaining work = consolidate, broaden to more assets, polish, **measure**, and write.

## Guiding principles (carried from the campaign)
- **Validation stays bit-exact / unbiased.** Beauty/robustness knobs are opt-in, default-off.
- **Measure, don't extrapolate.** Any thesis number comes from a fresh run, not compounded deltas.
- **Work-removal is the perf lever** here (latency-bound on erf dependency chains, not occupancy).
- **One feature per branch; user gates merges/pushes; main stays shippable.**

---

## MAIN SEQUENCE (do roughly in order; dependencies noted)

### Phase 0 — Consolidate & merge (foundation)
- [ ] Decide merge order / consolidate the 5 open branches (trivial `constants.cuh`/`raygen.cuh`/
      `FINDINGS.md` conflicts): `incremental-active-prims` (~16%), `fast-erf` (~1.5%),
      `showcase-quality` (firefly+filter), `findings-sobol-rgb` (doc), `denoiser` (doc).
- [ ] PR + merge them to `main` (user-gated push).
- [ ] Delete harvested dead branches (`a1-per-step-rb`, `wavefront` once its findings are captured —
      but see Track B: keep `wavefront` if still exploring).
- **Exit:** `main` carries all validated wins; `validate_ladder.sh` green on merged main.

### Phase 1 — Runtime flag batch (unblocks parity + asset work + two-mode rendering)
- [ ] `--max-depth` (was `MAX_BOUNCES`) — kills CUDA-vs-Mitsuba rebuild asymmetry
- [ ] `--hg-g` (was `HG_G`) — per-asset anisotropy without rebuild (touches phase fns)
- [ ] `--rr-depth` (+ disable RR) — Mitsuba RR-parity
- [ ] `--firefly-clamp` (was `FIREFLY_CLAMP_LUMINANCE`)
- [ ] `--filter-stddev` (was `PIXEL_FILTER_STDDEV`; box↔Gaussian)
- [ ] **Render-header log** of all active settings (depth, g, rr, filter, clamp, denoise) → self-
      documenting EXRs (can't accidentally quote a beauty render as a validation number)
- [ ] Keep estimator toggles (`NEE`/`MIS`/`ANALYTIC_DIRECT`) + memory caps compile-time
- **Exit:** one binary renders validation / beauty / both-modes via flags; defaults bit-identical.

### Phase 2 — Correctness & robustness
- [ ] **Bunny NaN** at σ=7.5 — diagnose + fix (blocks all solid-object/bunny assets)
- [ ] **Perspective camera vertical flip** — 1-line `pixel_dv_` sign (showcase camera)
- [ ] **RGB-albedo residual** root cause — narrowed to estimator×colored-albedo (§8.14); pin or
      document as accepted sub-percent
- [ ] Low-σ interior check (§8.13) — finish the σ=2 gray-interior comparison
- **Exit:** more assets renderable; right-side-up showcase; no open correctness surprises.

### Phase 3 — Measured re-benchmark (THESIS NUMBERS)
- [ ] Re-run the §8.17 equal-quality benchmark on the fully-optimized merged build
- [ ] Replace the extrapolated ~6×/~32× with **measured** figures (cloud+meadow, RTX 3090)
- [ ] Lock clocks if possible / use tight-interleaved method for stable numbers
- **Exit:** a defensible, measured speed/quality table for the thesis.

### Phase 4 — Asset generalization ("method, not just the cloud")
- [ ] Extract + npy→PLY convert **WDAS Disney cloud** first (`wdas4_1_small` or `wdas8_dense` —
      scattering, no new features, famous benchmark)
- [ ] Scene entry / `SG_PLY` path + camera framing; render vs meadow
- [ ] Matched **Mitsuba reference** per asset → validate ≤1e-4 (reuse `--hg-g`/`--max-depth`)
- [ ] Then `smoke`/`smoke_gauss`, bunny variants (after Phase 2 NaN fix)
- [ ] Note disk cost (~0.5–1.8 GB/zip) — be selective
- **Depends on:** Phase 1 (flags), Phase 2 (bunny NaN). **Exit:** ≥2–3 assets validated + showcased.

### Phase 5 — Emissive media (DECISION-GATED)
- [ ] **Ask Jorge** whether fire/explosion (embergen, fire, tornado) are in thesis scope
- [ ] If yes: add emission term to `Primitive` + raygen (emission contribution along path),
      validate energy + vs Mitsuba volpath emission
- [ ] Unlocks embergen / fire / tornado assets
- **Depends on:** Jorge's answer. **Exit:** emissive assets render correctly, or documented as out-of-scope.

### Phase 6 — Adaptive sampling (OPTIONAL, #56)
- [ ] Welford M2 infra exists but gated off; debug + validate (conditional variance-buffer alloc)
- [ ] Equal-quality gain measured; keep only if it wins
- **Exit:** optional feature shipped or documented as deferred.

### Phase 7 — Final renders & showcase
- [ ] Regenerate showcase bundle, right-side-up, **both modes** (validation/box vs beauty/denoised)
- [ ] Money-shot figures for each validated asset

---

## TRACK B — Wavefront (PARALLEL / "on the side", exploratory)
Run alongside thesis writing; **must not block** the main sequence or thesis.
- [ ] **Reality check first:** the wavefront's occupancy premise is *weakened* (reg-cap sweep null;
      kernel latency-bound on dependency chains, not occupancy-starved — §8.21, memory note).
- [ ] Concrete first step = **primary-ray HitBuffer fusion** (the one structural cost left): try to
      drop the 128-deep buffer from raygen's frame; this is the wavefront-adjacent lever.
- [ ] If pursuing the full split: Stage 2a (RayState global + host loop) → 2b (escape kernel) → 2c
      (measure), each validated against the monolith ≤1e-4.
- [ ] **Time-box it** with explicit bail criteria (per the campaign's discipline). Document result
      either way (win or proven-dead) in FINDINGS — a clean negative is thesis-worthy too.
- **Exit:** either a measured speedup merged, or a documented "monolith is the defensible final
  state" (the doc's own fallback).

---

## TRACK C — Thesis writing (FINAL phase; some sections can start mid-way)
Comes *after* most of the bullets above, BUT the user has a thesis shape + initial draft of
**abstract / background / current-work** — those are not engineering-dependent and can be shared &
refined earlier (e.g., during Phase 4–6 lulls / alongside Track B).
- [ ] **Share + integrate the existing draft** (abstract / background / current work) — user provides
- [ ] **Method chapter:** the DSYG Gaussian primitives, OptiX anyhit-single-descent design, analytic
      erf integration, ADT/argmin free-flight, NEE/MIS, analytic-direct RB
- [ ] **Validation chapter:** the ≤1e-4 ladder, furnace energy invariant, the 3 real bugs found+fixed
      (env flip, HG sign, MIS eval sign), the methodology lessons
- [ ] **Optimization chapter:** the work-removal thesis + every measured win AND null (shadow 15×,
      fusion 3%, dedup 8%, incremental scan 16%, erf 1.5%; nulls: occupancy/reg-cap, Sobol, A1, τ
      early-out) — the nulls are a strength (shows rigor)
- [ ] **Results chapter:** measured vs-Mitsuba table (Phase 3), with/without-denoiser figures,
      per-asset generalization (Phase 4), showcase money shots
- [ ] **Discussion/limitations:** RGB residual, 3σ truncation, emission scope, wavefront outcome
- **Source material:** `FINDINGS.md` §8 is effectively a lab notebook → mine it for chapters.

---

## Dependency summary
```
Phase 0 (merge) ──► Phase 1 (flags) ──► Phase 4 (assets) ──► Phase 7 (final renders)
                          │                  ▲
Phase 2 (correctness) ────┴──────────────────┘ (bunny NaN gates bunny assets)
Phase 3 (benchmark) ── independent, needs merged main ──► Results chapter
Phase 5 (emission) ── gated on Jorge ──► emissive assets
Track B (wavefront) ── parallel, non-blocking
Track C (writing) ── final; abstract/background can start anytime
```

## Recommended next action
Phase 0 (merge the wins) + Phase 1 (flag batch) — they unblock everything downstream and are low-risk.
Then Phase 2 bunny-NaN + Phase 4 WDAS cloud (first new asset). Track B (wavefront) can start in
parallel whenever; thesis draft sharing can begin once Phase 3–4 give real numbers/figures.
