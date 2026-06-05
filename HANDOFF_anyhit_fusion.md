# Handoff: anyhit-fusion optimization (eliminate the shadow-ray hit buffer)

**Branch:** `feature/anyhit-transmittance-fusion` (off `main`). Start here. Nothing implemented yet —
this is the plan + all context. Task tracker: **#61**.

## TL;DR of what to build
Fuse the shadow-ray `optical_depth` integration **into traversal**: a transmittance-mode `anyhit`
accumulates τ in the ray payload during the single GAS descent, so the 128-deep `HitBuffer` is never
filled for shadow rays. Goal: remove "the EXPENSIVE per-ray buffer" (`constants.cuh` comment) that
caps occupancy at ~30%, lifting occupancy → a further speedup **on top of** the already-merged 15×.
**This is a measured BET, not a sure win** — see Risks.

## Context you need (don't re-derive)
- The big win already shipped: `compute_transmittance_to_env` (device/core/sampling.cuh) was rewritten
  O(A²) segment-march → **O(A) per-prim integral**, ~15× faster, **merged to main** (PR #3). See
  FINDINGS §8.16. Post-opt benchmark §8.17: CUDA-MIS is now **24× faster AND correct vs Mitsuba-MIS**,
  4.5× faster/spp than Mitsuba-analog. Current cloud cam0 48spp = **6.5 s** (was 98.7 s).
- **Why the pipeline is anyhit-only (the user's design, PRESERVE IT):** one `optixTrace` per ray; the
  `anyhit` collects all K entry hits in a *single* GAS descent (`optixIgnoreIntersection` continues
  traversal) and exits are computed analytically — strictly better than ray-marching K descents. The
  fusion KEEPS this (still one descent); it just moves the integration inline instead of buffering.
- Profiling that motivated this: kernel is **latency-bound** (SM 30% / DRAM 11% / occupancy 30% / 114
  regs). ~85% of frame was the NEE shadow-ray integration; the 128-deep `HitBuffer` is the LMEM hog.

## Design (recommended: single anyhit + a mode flag — NO host/SBT changes)
**1. `include/thesis/device/payloads/anyhit.h`** — add a `mode` to the AnyHit payload:
   - `Count = 1 + 3` (was `1 + 2`): tag(slot0) + ptr_low(1) + ptr_high(2) + mode(3).
   - `pack_impl`: `out[0]=ptr_low; out[1]=ptr_high; out[2]=mode;`  `unpack_impl` reverse.
   - `MODE_COLLECT=0` (ptr = `HitBuffer*`, current behavior), `MODE_TRANSMITTANCE=1` (ptr = `float* τ`).
   - NB `optixTrace` in trace.cuh wires exactly `ps[0..3]` (4 regs) — mode fits as the 4th. Miss payload
     is also 4 (tag+rgb); fine because τ lives in **local memory** (via the ptr), so the miss shader
     clobbering payload regs does not touch τ.

**2. `device/core/trace.cuh`**
   - `trace_ch_collect`: set `payload.mode = MODE_COLLECT;`
   - Add `trace_transmittance(ray, t_min, t_max) -> float`:
     ```cpp
     float tau = 0.0f;
     payloads::AnyHit p; pack_ptr(&tau, p.buffer_ptr_low, p.buffer_ptr_high);
     p.mode = AnyHit::MODE_TRANSMITTANCE;
     uint ps[payloads::MAX_PAYLOADS]{}; p.pack(ps);
     optixTrace(launch_params.ias_handle_, ray.origin_, ray.direction_, t_min, t_max, 0.0f,
                VISIBILITY_ALL, OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT | OPTIX_RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
                0, 1, 0, ps[0],ps[1],ps[2],ps[3]);
     return tau;   // accumulated by the anyhit during traversal
     ```

**3. `device/entry/anyhit.cuh`** — branch on mode (needs new includes: `core/sampling.cuh` or just
   `primitive.h` + `common/geometry/intersection.h` + `core/launch_params.cuh` + math):
   ```cpp
   auto p = payloads::AnyHit::unpackFromOptix();
   if (p.mode == AnyHit::MODE_TRANSMITTANCE) {
       const geometry::Ray ray(optixGetWorldRayOrigin(), optixGetWorldRayDirection());
       const float t_entry = optixGetRayTmax();
       const auto& prim = launch_params.primitives_[optixGetInstanceId()];
       const auto w = prim.transform_dir_local(ray.direction_);
       const float t_exit = common::geometry::compute_exit_from_entry(ray, t_entry, prim, math::length2(w));
       if (t_exit > t_entry) {
           float* tau = unpack_ptr<float>(p.buffer_ptr_low, p.buffer_ptr_high);
           *tau += prim.optical_depth(ray, t_entry, t_exit);
       }
       optixIgnoreIntersection();
       return;
   }
   // else: existing MODE_COLLECT path (append hit) — unchanged
   ```
   v1: **skip** the `MAX_OPTICAL_DEPTH` early-out (don't `optixTerminateRay`); add it only if v1 wins.

**4. `device/core/sampling.cuh` `compute_transmittance_to_env`** — keep the `active_prims`
   (origin-inside) loop EXACTLY as is (those prims have no forward entry hit, can't be done in anyhit),
   then replace the `collect_hits` + hit-loop with `tau += trace_transmittance(shadow_ray, 0.0f, INF_F);`
   Drop the `HitBuffer& hit_buffer` parameter.

**5. `device/entry/raygen.cuh`** — drop the `hit_buffer` arg at the ~4 `compute_transmittance_to_env`
   call sites (analytic-direct ~:140 and the NEE/MIS strategies ~:199/210/220). `hit_buffer` is still
   needed for the PRIMARY ray (`sample_scattering_event`), so keep it declared; just don't pass it here.

## Validate (mandatory — same bar as §8.16)
1. Build: `cmake --build build`. **If you add/remove device files, `cmake -S . -B build` first** (the
   optixir target globs device headers; stale glob → "missing file" errors).
2. **Correctness** (integration math is identical → output must match): render cloud cam0 128spp seed0
   on this branch vs `main` (checkout main, build, render, diff). Expect mean Δ≈0, mean-abs ~1e-7, max
   ~1e-3 (float order). Furnace: `SG_ALBEDO=1.0 ... single_gaussian_validation σ=4 spp1024` → mean ~1.0.
   (Pattern: see the now-deleted `renders/shadow_opt_bundle_*` approach / FINDINGS §8.16.)
3. **Perf A/B:** time cloud cam0 48 spp vs the current **6.5 s**. Also `ncu` the raygen registers/
   occupancy (find the `optixLaunch` kernel by its ~114 regs / full grid; see `tools/refs/wf_reg_driver.sh`
   on branch `feature/wavefront` for the ncu invocation). Win = faster wall-clock + higher occupancy.

## Risks / when to BAIL
- **The bet:** the transmittance anyhit now does erf `optical_depth` *inside traversal*. A heavier
  anyhit can throttle the GAS descent (RT cores wait on it) — the user's collect-anyhit is fast
  *because* it's light. Net (occupancy gain − heavier anyhit) is **unknown until measured**. If v1 is
  not clearly faster, **revert** — it's a branch, no harm. Don't force it.
- Don't break the primary/scatter path: `MODE_COLLECT` must be byte-for-byte the old behavior.
- Keep it ONE anyhit + mode flag (above). A separate `__anyhit__transmittance` program would need a
  2nd hitgroup + SBT/pipeline edits in `src/thesis/host/app/renderer.cpp::createPipeline` — more risk;
  avoid unless the mode-branch approach has a problem.

## Repo state at handoff
- `main`: has the merged 15× optimization + all validation (FINDINGS §8.16/§8.17). Local main is ahead
  of origin until the user pushes.
- Branches: `feature/anyhit-transmittance-fusion` (this work), `feature/a1-per-step-rb` (A1 dead-end,
  artifact), `feature/wavefront` (wavefront analysis + RayState scaffold + diagnostic scripts).
- Workflow: NO AI mentions in commit messages; user pushes/merges (your `git push`/`git branch -D` are
  blocked); branch per change.
- Memory: `project_kernel_validation`, `project_wavefront_status`, `project_a1_dead_end` are current.
