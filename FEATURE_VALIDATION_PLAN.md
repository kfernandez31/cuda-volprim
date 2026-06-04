# Plan: Validate the OFF/untested features (real HDR env, HG g≠0, MIS, env-IS) vs Mitsuba

> This is the active dev plan for THIS week. After this, dev closes and the rest of the
> month is thesis writing. Wavefront/perf comes AFTER (see PLAN.md, TODO.md). Evidence this
> rests on: FINDINGS.md §8. Density/asset facts: memory `reference_asset_density_scales`.

## Context

The scattering path is validated end-to-end vs Mitsuba `volprim_prb` (FINDINGS §8), but **only
under a constant white env, isotropic phase (g=0), MIS off, grey albedo.** Several capabilities
exist in code yet are OFF or never exercised. Before the big wavefront perf rewrite, close the
remaining Mitsuba **feature/validation** gaps. This also yields the *nuanced cloud look* (real
environment lighting + forward scattering) for thesis beauty figures, plus a g-sweep / MIS-
variance result.

**Decisions locked by user:** HG_G and ENABLE_MIS stay compile-time flags (rebuild device per
config — do NOT promote to runtime). Colored per-channel RGB *albedo* is DEFERRED (the meadow
HDR already validates RGB *lighting* transport; the float3 albedo path has no cross-channel
coupling → low risk; note as "supported, validation deferred").

## Methodology (reuse what's proven — do NOT reinvent)

- **Trustworthy reference = Mitsuba `volprim_prb` ANALOG (`use_nee=False`).** Its NEE fails the
  furnace test by +6.5% (FINDINGS §8.1) → NEE-on is NOT ground truth. For meadow, analog
  continuation rays hit the env directly → unbiased.
- **Energy oracle = the furnace test** (recreate `/tmp/furnace_check.py` from FINDINGS §8.1 if
  gone): conservative medium (albedo=1) in a **constant** env must render perfectly flat — for
  ANY phase function or MIS setting. Reference-free. Gate for every new toggle.
- **Correctness numbers = multi-seed CUDA\* vs Mitsuba-analog M\***, noise-free systematic via
  `tools/refs/cloud_systematic_direct.py`; cross-RMSE 1/√spp via `cloud_scatter_metrics.py`;
  per-region/bin via `tools/refs/cmp_scatter.py`. `--seed` flag exists on test_runner.
  Multi-seed render pattern: `tools/refs/cloud_converge_cuda.sh` / `cloud_converge_mitsuba.sh`
  (idempotent/resumable). Run long renders detached: `setsid nohup … &`.
- **Compile-time configs:** edit `device/core/constants.cuh`, then `cmake --build build` (optixir
  auto-rebuilds on header change). Validate one config at a time.

## Critical convention facts (from code exploration — verify before trusting)
- CUDA env equirect (`include/thesis/device/params/environment_map.h:36-61`):
  `u = atan2(z,x)/(2π)+0.5`, `v = acos(y)/π`; +Y up (v=0 top), +X→u=0.5, +Z→u=0.75; loader
  vertical-flips (`src/thesis/host/utils/io/hdr.cpp:202`).
- `env_is` (`device/core/sampling.cuh:152-243`) IS a real 2D-luminance-CDF importance sampler
  (Rec.709 × sinθ), built in `include/thesis/host/params/environment_map.h:34-101` — but it has
  **only ever run on a uniform env** (degenerates to uniform). Meadow is its first real exercise.
- HG phase (`device/core/sampling.cuh:39-130`): correct sample+pdf for g≠0; just HG_G=0.
- MIS (`device/entry/raygen.cuh:193-214`, `mis_balance` sampling.cuh:247): complete + sound; off.
- Albedo is genuine `float3` end-to-end (no scalar collapse). Current toggles
  (`device/core/constants.cuh`): `ENABLE_NEE=true`, `ENABLE_MIS=false`,
  `ENABLE_ANALYTIC_DIRECT=true`, `HG_G=0.0`.
- Mitsuba supports `{"type":"envmap","filename":...}`, integrator `phasefunction {"type":"hg","g":…}`,
  per-prim RGB albedo tensor. `tools/refs/with_jorge_mitsuba.sh` wraps Jorge's build.
- Assets exist: `assets/meadow_2_4k.hdr`, `assets/white_constant.hdr`.

---

## Workstream 0 — Env-map orientation parity (PREREQUISITE; #1 risk)
A meadow comparison is meaningless unless CUDA and Mitsuba sample the *same* env direction→pixel.
Mitsuba's `envmap` has its own convention + a `to_world` rotation DOF.
- **Note:** an orthographic camera makes ALL background rays parallel → background = env(one
  direction) = flat → useless for orientation. **Calibration needs a PERSPECTIVE camera**
  (background shows the whole map). Check `include/thesis/host/params/camera.h` for a perspective
  constructor (renderer already supports perspective via `is_orthographic()`).
- **Calibrate:** render the env DIRECTLY (perspective camera, negligible/one tiny far Gaussian)
  on both sides; find the Mitsuba `envmap to_world` rotation (likely 90° about Y and/or a flip)
  that makes the two backgrounds match pixel-for-pixel. Bake that `to_world` into the scripts.
- **Unit-check `env_is`:** (a) sample→pdf round-trip consistency; (b) samples concentrate on
  bright meadow regions; (c) `env_is::pdf` direction convention matches `env_map.sample`.
- If exact match is impossible, document the residual rotation and proceed (constant offset).

## Workstream 1 — Real HDR env (meadow) validation
- **CUDA:** add a `SG_ENV` env var (default white_constant; `SG_ENV=meadow` →
  `env_map_override = "assets/meadow_2_4k.hdr"`), read in `test/scenes/single_gaussian.cpp` and
  `test/scenes/cloud_validation.cpp`. Perspective-camera variant for the single-Gaussian env
  test; cloud keeps its ortho cameras (the cloud *body* is still lit by the full meadow under
  ortho — only the flat background sees one direction).
- **Mitsuba:** `render_*_via_prb.py` gain an `envmap` emitter (calibrated `to_world`) selected by
  `SG_ENV`, replacing the constant emitter; keep `use_nee=False`.
- **Validate:** converged multi-seed CUDA\* vs M\*; systematic ≤ ~1e-4; cross-RMSE 1/√spp.
  Exercises env path + `env_is` (real HDR) + NEE under real lighting. (Furnace N/A: non-constant.)

## Workstream 2 — HG anisotropy (g≠0)
- Rebuild with `HG_G = 0.85` (cloud-typical forward scattering).
- **Furnace(g=0.85, albedo=1, constant env) → must stay flat** (reference-free energy gate).
- CUDA(g=0.85) vs Mitsuba-analog with `phasefunction {"type":"hg","g":0.85}` — constant env
  first (clean), then meadow. Single Gaussian → cloud. Systematic ≤ ~1e-4.

## Workstream 3 — MIS (highest-risk path — gate hard)
- Rebuild with `ENABLE_MIS=true` (test with meadow and/or g≠0, where env-IS/MIS actually helps).
- **Furnace(MIS on, albedo=1, constant env) → must stay flat.** KEY GATE: MIS is the exact kind
  of path that broke in Mitsuba (+6.5%). If it fails, fix before trusting/shipping.
- CUDA(MIS on, meadow) vs Mitsuba-analog: MIS is variance reduction → must converge to the SAME
  image as analog. Systematic ≤ ~1e-4. ALSO measure the variance win: noise const kC with MIS
  vs NEE-only on meadow (a nice thesis result).

## Workstream 4 — Combined "money shot" + thesis figures
- Cloud + meadow + HG g≈0.85 (+ MIS) = the nuanced look. Validate vs Mitsuba-analog; render the
  beauty figure(s) with a stated correctness claim. Optional g-sweep panel (rebuild per g).

## Workstream 5 — Regression gate + FINDINGS + handoff
- Build a `validate_ladder.sh`-style gate (per PLAN.md) incl. furnace-HG, furnace-MIS, and the
  meadow systematic rungs, so future changes auto-check.
- Record in FINDINGS.md §8 (new subsections) with measured systematics + the MIS variance
  result. Update TODO.md (features validated; RGB-albedo deferred; wavefront next).

## Critical files
- Toggles: `device/core/constants.cuh` (HG_G, ENABLE_MIS) → `cmake --build build`.
- Scenes: `test/scenes/single_gaussian.cpp`, `test/scenes/cloud_validation.cpp` (add `SG_ENV`;
  perspective-camera variant), `test/test_runner.cpp` (dispatch).
- Camera: `include/thesis/host/params/camera.h` (perspective constructor).
- Mitsuba refs: `tools/refs/render_single_gaussian_via_prb.py`,
  `tools/refs/render_cluster_via_prb.py`, `tools/refs/render_cloud_prb_absorption.py`
  (add `SG_ENV` envmap emitter + calibrated `to_world`; `SG_HG_G` hg phasefunction).
- Reuse: `tools/refs/cmp_scatter.py`, `cloud_systematic_direct.py`, `cloud_scatter_metrics.py`,
  `cloud_scatter_study.sh`, `cloud_converge_*.sh`, `with_jorge_mitsuba.sh`, furnace check.

## Verification (end-to-end)
1. **Orientation:** env-only backgrounds (perspective) match CUDA vs Mitsuba pixel-for-pixel.
2. **Energy gates:** furnace flat (mean≈1.0, residual=noise) for g=0.85 and for MIS on.
3. **env_is:** round-trip pdf consistency; importance concentrates on bright meadow regions.
4. **Correctness:** converged multi-seed CUDA\* vs Mitsuba-analog M\* systematic ≤ ~1e-4 for:
   meadow (single→cloud), HG g=0.85 (single→cloud), MIS-on (meadow). Cross-RMSE tracks 1/√spp.
5. **MIS variance:** measured kC(MIS) < kC(NEE-only) on meadow.
6. **Beauty:** cloud+meadow+HG figure rendered and matched.
7. main stays shippable; each config validated before any default flips.

## Risks
- **Orientation parity (#1):** mitigate with explicit perspective calibration; document residual.
- **MIS energy bug:** furnace gate catches it; do NOT ship MIS-on if furnace fails.
- **env_is never tested on real HDR:** unit check + meadow match catch CDF/convention bugs.
- **HG near g→1:** numerical edge; use g=0.85 not 0.99; furnace + match catch it.
- **Time (1 week):** priority = meadow + HG (the look) → MIS → beauty shot. Wavefront is OUT.

## Out of scope (named, not done now)
- Wavefront / occupancy rewrite (next phase; PLAN.md, TODO.md B3).
- Colored per-channel RGB *albedo* (deferred; meadow covers RGB lighting transport).
- Differentiability / inverse rendering (structural; forward-only renderer by design).
