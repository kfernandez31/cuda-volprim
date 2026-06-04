# Hybrid Wavefront Rewrite — concrete implementation plan (branch `feature/wavefront`)

**Goal:** cut the ~1.93× per-spp *throughput* gap vs Mitsuba (FINDINGS §8.5) by raising GPU occupancy.
The monolithic raygen is register-bound (`hybrid-wavefront-plan.md`: 114 regs/thread → 32.7% occupancy,
ceiling 35.4%); the escape path (EventBuffer + exit recompute + event sort + segment integration) is
the heaviest code. Splitting it out should drop raygen registers (~70–80) → ~50–60% occupancy.

> This is the OTHER half of the equal-quality gap. The variance half (A1) is a proven dead end
> (`A1_INVESTIGATION.md`). NB the renderer already match-and-beats Mitsuba on the env-map showcase
> (§8.11); wavefront widens that win and helps flat-lit throughput — it is polish, not a fix.

## Architecture map (verified, with insertion points)
- **Host render loop:** `src/thesis/host/app/renderer.cpp:272-328` `Renderer::render()` — batches over
  spp; each batch one `pipeline_.launch(... width,height,1)`. `BATCH_SIZE=16` (renderer.cpp:32).
- **Kernel:** `device/entry/raygen.cuh:20-288` — per-pixel; **outer loop over samples** (`:71`,
  `batch_size_`), **inner loop over bounces** (`:105`, `MAX_BOUNCES`), Welford accumulate (`:259-271`).
- **LaunchParams:** `include/thesis/common/params/launch_params.h` (ias, camera, env_map, image,
  primitives, seed); device symbol `device/core/launch_params.cuh:5`; uploaded via
  `cuda::AsyncBuffer launch_params_` (`renderer.cpp:48`, `updateDynamicParams` `:200`).
- **Image/accum:** device struct `include/thesis/device/params/image.h` (mean_/sample_counts_/variance_/
  AOVs, W×H `AsyncBuffer`s). Welford lives per-pixel in these buffers.
- **Pipeline/SBT:** `renderer.cpp:207-270`, `maxTraceDepth=1` (already one trace per launch — good).
- **Buffer RAII:** `AsyncBuffer<T>` (`include/thesis/host/cuda/async_buffer.h`), `cudaMallocAsync` on a
  stream; allocate like `primitives_`.
- **2nd CUDA kernel:** add a `.cu` under `device/`, compile via `cmake/Device.cmake`, launch with
  `cuLaunchKernel` on `streams_[StreamKind::Main]` (no OptiX program needed for the escape stage).

## The hard design decision: sample loop + launch count
Naive Phase 1 (one ray/pixel, host loops samples×bounces) = `spp × MAX_BOUNCES` launches
(256×128 ≈ 33k) → launch overhead could dominate and make the experiment *look* bad even if the
architecture is sound. Mitigations, in order of preference:
1. **Persistent megakernel-free wavefront with compaction** (the real target): RayState pool of
   W×H rays = one sample/pixel in flight; bounce loop with an `alive_count` and stream compaction so
   late bounces launch only over survivors. Per sample: ≤MAX_BOUNCES launches, most tiny.
2. **Keep the sample loop in the host, early-out dead rays** (Phase-1 simplest): correct but the
   33k-launch overhead is the thing we measure. Acceptable as the *gate* (does global RayState +
   multi-launch even pay?), not as the final design.
Decision: implement Phase 1 as (2) to isolate the global-memory/launch cost (the doc's Risk #1),
then move to (1) only if (2)'s overhead is survivable.

## Staged implementation (each stage builds + validates before the next)
**Stage 0 — profiling gate — DONE, PASSES (2026-06-04).** `ncu` on the raygen (it shows as the
`optixLaunch` kernel, ID 9, grid 256×256): **114 registers/thread** — confirms the doc's premise.
On the RTX 3090 (sm_86, 65536 regs/SM): 65536/114 ≈ 575 threads/SM ≈ **~37% occupancy ceiling**,
register-bound (matches the doc's 35.4% theoretical / 32.7% achieved). So splitting the escape path
to drop raygen registers is a legitimate occupancy lever → **proceed.** (ncu can't name OptiX
programs directly — identify raygen by its 114-reg / full-grid signature; revisit in Stage 3 to
measure the post-split register drop. TODO Stage 3: also confirm the *escape path* is the dominant
register driver, not the trace/scatter common path — if it isn't, the split won't help.)

**Stage 1 — RayState + plumbing (safe, no behavior change).**
- New `include/thesis/device/params/ray_state.h`: `RayState { float3 origin,direction,throughput,
  radiance; PCG32 rng; PrimsSet active_prims; uint8 bounce; bool alive; }` (~per doc).
- Add `RayState* ray_states_` to LaunchParams; allocate `AsyncBuffer<RayState>(W*H)` in Renderer.
- Build only; buffer unused → renders unchanged. **Validate:** bit-identical output.

**Stage 2 — move the bounce loop to the host (the real change).**
- Split raygen into: `__raygen__init` (camera ray + rng + reset state, one per pixel per sample) and
  `__raygen__bounce` (read RayState, do ONE bounce — trace + scatter + NEE/MIS shade + write back;
  on escape/RR set `alive=false`).
- Host `render()`: per batch → per sample → init launch → `for bounce: bounce launch` → accumulate
  launch (Welford add of `radiance` into `mean_`). Reuse the existing accumulate math from raygen
  `:259-271`.
- **Validate:** furnace flat + systematic vs the monolith ≤~1e-4 (reuse `validate_ladder.sh`); a few
  cloud cams bit-close. Measure wall-clock vs monolith (expect SLOWER here — no kernel split yet;
  this stage only proves correctness + measures global-state overhead).

**Stage 3 — extract the escape kernel (the occupancy win).**
- Per `hybrid-wavefront-plan.md`: on escape, persist hit_buffer + enqueue `EscapeEntry`; a pure-CUDA
  `process_escapes` kernel (`__launch_bounds__`) does the segment integration. Raygen loses the
  EventBuffer/exit-recompute/sort/integration code → registers drop.
- **Validate:** same gates; then `ncu` raygen registers (expect 114→~70-80) + occupancy (→~50-60%);
  wall-clock vs monolith across escape/scatter-ratio scenes.

**Stage 4 — true wavefront compaction** (design (1)) only if Stages 2-3 show promise.

## Validation (every stage)
`bash tools/refs/validate_ladder.sh` (furnace energy + meadow systematic) + cloud cam0/6 systematic
≤~1e-4 vs the committed monolith baseline. The refactor is correctness-preserving by construction →
ANY diff beyond noise = a bug. main stays shippable; `feature/wavefront` holds the experiment.

## Fallback (documented as a finding if it fails)
If Stage 2 global-state overhead is catastrophic, or Stage 3 doesn't raise occupancy (OptiX JIT may
keep 114 regs and just spill less), STOP and document the occupancy ceiling + its cause as the result
— the monolith at 32% with PCG32/LMEM opts is a defensible final state (per the doc's own fallback).

## Realistic effort (nonstop auto-mode): Stage 1 ~0.5d · Stage 2 ~2d · Stage 3 ~2d · Stage 4 ~1-2d.
GPU validation/debug latency dominates; ~1–1.5 weeks. This file + Stage-1 code are the structured start.
