# Cap Calibration by Measurement — Design

**Date:** 2026-06-12
**Branch:** `feature/cap-calibration` (worktree `.claude/worktrees/cap-calibration`, off `main` @ `f62101a`)
**Status:** approved in brainstorm (A + C + clamp cherry-pick); successor to the abandoned cap-free streaming line (`feature/cap-free-streaming`, kept unmerged as the documented negative result — see `results/campaign/capfree_summary.md` on that branch).

## 1. Problem

The buffered renderer stays (the streaming alternative measured 12–22 % slower on small assets). Its two compile-time caps (`MAX_ACTIVE_PRIMS`, `HIT_BUFFER_CAPACITY` in `device/core/constants.cuh`) are currently sized by `scripts/tools/estimate_caps.py` — offline Monte-Carlo geometry (random points / random lines in the AABB). Known limitations, calibrated by the capfree campaign:

- It estimates a **camera-independent whole-bbox bound**, which over-sizes: bunny's active cap suggested 320 while no render ever exceeded 128. Oversizing costs real time at the current operating point (universal 512/512 vs tuned: cloud +13 %, explosion +7 %, tornado/bunny ~+3 % — `capfree_b_perf.md` on the streaming branch).
- Its max-statistics come from random sampling with no convergence diagnostic — sufficiency is verified only after the fact by the runtime overflow counters.
- The workflow is manual: run script → edit constants → rebuild → stress-verify.

Decision from the brainstorm: caps should be **per-workload tight** — sized for the actual planned renders (asset + camera set + spp), not for any conceivable ray.

## 2. Goal and non-goals

**Goal:** replace estimation with **direct measurement**: the renderer counts, while rendering the real workload, the exact maxima the caps must cover, and a one-command wrapper turns that into calibrated constants + a verified rebuild. Plus one independent correctness fix from the campaign (the sub-entry clamp).

**Non-goals:** no change to the sampling algorithm or render output (measurement is observation-only); no runtime-sized buffers; no removal of `estimate_caps.py` (it stays as the documented camera-independent ceiling); no thesis text in this branch.

## 3. Key insight

Both capped quantities are directly observable during a normal render, with **no dependence on the current cap values**:

- **Hits per ray:** the COLLECT anyhit is invoked for *every* entry hit regardless of buffer fullness (on overflow it drops the entry but keeps traversing). Counting invocations therefore measures true demand even on a binary whose buffer is too small — no chicken-and-egg.
- **Point overlap at scatter points:** an O(N) `point_inside_bvh_bound` scan at the scatter position (the same predicate the bounce-0 scan uses) counts the true active-set requirement even past the `CompactSet` capacity.

So a stock binary can measure any asset; the measured maxima are exact for the measured workload; the existing render-time overflow counters remain the safety net for unmeasured seeds/views.

## 4. Design

### 4.1 Device: measurement counters (observation-only, runtime-gated)

- `HitBufferSoA` gains a `uint32_t total_seen_` counter; the COLLECT anyhit increments it unconditionally on every invocation (one register-add next to the existing push; the push/drop logic is untouched). `clear()` resets it.
- `RenderParams` gains `bool measure_caps_` (default false; set by the new CLI flag).
- In `sample_scattering_event`, under `if (launch_params.render_.measure_caps_)` (uniform branch, dead-cheap when off):
  - after `collect_hits`: `atomicMax(&launch_params.measure_buf_[MEASURE_HIT_MAX], hit_buffer.total_seen_)`;
  - after the active-set rebuild (scatter case): O(N) containment count at `event.position_` over all primitives, then `atomicMax(&launch_params.measure_buf_[MEASURE_ACTIVE_MAX], count)`. The count is computed by the scan, not read from `final_active_prims` (whose size is clipped by the current cap). Also taken at bounce 0 for the camera-origin set (the scan already exists there; reuse its loop count).
- `launch_params` gains `uint32_t* measure_buf_` (2 slots, same `AsyncBuffer` pattern as `overflow_counter_`; null when measurement is off → guarded exactly like `overflow_counter_`).

Bias note: measurement must not perturb the render — it only reads state and writes to a dedicated buffer; the RNG stream, buffers, and control flow are untouched. A measurement render's image is bit-identical to a normal render's (assertable in the test).

### 4.2 Host: `--measure-caps`

- CLI flag (Config + arg parsing, alongside existing flags) → sets `render_.measure_caps_`, allocates `measure_buf_`, zeroes it.
- After the render, read back and report (always-printed block, mirroring the overflow readout):

```
Cap measurement: max hits/ray = H, max point-overlap = A
Suggested caps:  HIT_BUFFER_CAPACITY = ⌈1.125·H⌉₁₆, MAX_ACTIVE_PRIMS = ⌈1.125·A⌉₁₆
```

(⌈x⌉₁₆ = round up to the next multiple of 16; 1.125 margin covers seed-to-seed variation of the max — calibrated as adequate by re-measuring across seeds in validation, §6. The suggestion line is machine-parseable for the wrapper.)

### 4.3 Wrapper: `scripts/tools/calibrate_caps.sh`

One command per asset: `calibrate_caps.sh <asset> [spp] [seeds...]`:

1. Runs `test_runner --measure-caps` on the asset's scattering stress config (the binding stress per `caps_per_asset.md`: meadow env, albedo 0.9, diag view for PLY assets; the cloud uses its `cloud_asset_scattering` scene) for each seed (default 42 43), parses the suggestion lines, takes the max.
2. `sed`s the two constants in `device/core/constants.cuh`, rebuilds (`cmake --build build -j`).
3. Re-runs the stress WITHOUT `--measure-caps` and asserts `Cap check: 0 overflows`.
4. Prints a summary row: asset, measured H/A, suggested caps, verification verdict. Non-zero exit on any failure; restores `constants.cuh` (`git checkout`) on failure paths.

Asset → scene/config mapping lives in a small case-block at the top of the script (cloud, tornado, explosion, bunny; extensible).

### 4.4 Sibling fix: sub-entry clamp (cherry-pick from the campaign)

In `sample_scattering_event`'s hit loop, after `inv_cdf_segment`: clamp `t_scatter` to `hit_t` when `0 ≤ t_scatter < hit_t` (FP undershoot of the erf/erfinv round-trip). Without it, a χ≈0 draw (p≈2⁻³² per draw on the sequential RNG) yields a winner ULPs below its own entry, the rebuild's `hit_t > t_scatter_min` filter excludes the winning prim from its own active set, `evaluate_albedo` returns 0, and the path silently dies. Mechanism proven at bit level in the capfree campaign (`capfree_b_gate.md` on the streaming branch). Lands as its own commit; expected image impact ≤ a few pixels per render (the campaign measured ≤6 channel-values/scene for the equivalent patch).

## 5. Components touched

| Area | Files |
|---|---|
| Hit counter | `device/core/hit_record.cuh` |
| Measurement hooks | `device/core/sampling.cuh` (gated block ×2) |
| Launch params / render params | `device/core/launch_params.cuh` or the shared params headers (match where `overflow_counter_`/`RenderParams` actually live), `src/thesis/host/app/renderer.cpp` |
| CLI | host config/arg parsing (wherever `--ris` etc. are defined) |
| Wrapper | `scripts/tools/calibrate_caps.sh` (new) |
| Clamp | `device/core/sampling.cuh` (hit loop, separate commit) |

## 6. Validation

1. **No-perturbation:** a fixed-seed render with and without `--measure-caps` must be **bit-identical** (`exr_diff.py`), and `--all` suite stays 43/43.
2. **Clamp:** furnace PASS at 1024 spp; suite 43/43; fixed-seed cloud render vs pre-clamp differs at ≤ a-few-pixels scale (document the count — the χ≈0 event rate predicts ~0–6 channel-values per image).
3. **Measurement vs reality:** for all four assets, run the wrapper end-to-end. The measured maxima must be ≤ the estimator's whole-bbox suggestions (`caps_table.csv`) and the calibrated caps must produce `Cap check: 0 overflows` on the stress at *unmeasured* seeds (e.g., measure on 42/43, verify on 7) — this empirically validates the 1.125 margin.
4. **Deliverable table:** `results/campaign/cap_calibration.md` — per asset: estimator suggestion vs measured max vs calibrated cap vs verification verdict (thesis-ready: "geometric bound vs measured demand").

## 7. Acceptance

Work stays on `feature/cap-calibration` until the user reviews the validation results; merge to `main` only on explicit approval. After acceptance, the cap domain is closed and work returns to the planned thesis experiments.
