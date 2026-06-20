# SER remote-GPU experiment — runbook

**Goal.** Turn the §6 autopsy claim — *"Shader Execution Reordering (SER) is the one true hardware
lever for this divergence, but it is an Ada-generation feature, unavailable on the Ampere RTX 3090, so
it cannot be brought to bear"* — into a **measured** result, by enabling SER on a rented Ada/Blackwell
GPU and A/B-ing it against the divergence bottleneck on the cloud + bunny.

**Scope (decided):** SER only (path guiding is software, runs on the 3090, separate effort).
**Ambition (decided):** minimal thesis-grade A/B — one reorder at the dominant divergence point, not a
production restructure. Either outcome is a contribution:
- SER helps → "on SER-capable hardware the lever recovers X%, confirming the divergence diagnosis; the
  3090 simply lacks it."
- SER negligible → "even on capable hardware SER yields <X%, so the bottleneck is register pressure /
  global-load latency, not pure scheduling divergence" (sharpens the diagnosis).

All other thesis numbers stay pinned to the 3090; this is a single, clearly-labelled cross-arch probe.

---

## Hardware/software prerequisites — verify FIRST on SSH
- `nvidia-smi --query-gpu=name,compute_cap,driver_version --format=csv`
  - **Must be Ada (CC 8.9) or consumer/workstation Blackwell (CC 12.0).** RTX 4090/4080, RTX 6000 Ada,
    L4/L40/L40S, RTX 5090, RTX PRO Blackwell all qualify.
  - **Reject:** A100/H100/**B200** (no RT cores), and Ampere A10/A40/A6000/3090 (RT cores but **no SER** —
    `optixReorder` compiles but is a no-op, so a null A/B).
  - Driver: R555+ for OptiX 9 on Ada; **R570+** for Blackwell.
- CUDA 12.x toolkit, CMake, ninja, a C++20 compiler (match what `build/` used locally).
- OptiX **9.0** SDK headers (this repo is built against `OPTIX_VERSION 90000`; `optixReorder` lives in
  `optix_device.h`). Easiest: `rsync` the local `~/optix-dev` to the remote.

---

## Phase 0 — provision + transfer
1. Rent (Vast.ai / RunPod) an **RTX 4090** (Ada, simplest) or **RTX 5090** (Blackwell). Confirm CC as above.
2. Transfer to the box:
   - repo (`git clone` or `rsync -a ~/thesis`),
   - assets: `assets/models/{cloud,bunny}/*.ply`, `assets/environment_maps/meadow_2_4k.hdr`,
   - OptiX SDK headers (`~/optix-dev`).
   - (Banked EXR refs are gitignored and **not needed** — this is a perf experiment.)
3. Install toolchain; point CMake at the OptiX include dir (see `cmake/OptiX-IR.cmake`).

---

## Phase 1 — baseline build + correctness gate (do NOT skip)
1. Build the **stock** renderer (no SER) on the remote (`cmake … && ninja` as in `build/`).
2. Reproduce a known number to prove the port is correct *before* trusting any SER delta:
   - Render the cloud at the g1 config: `SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner
     --scene cloud_asset_scattering --sigma-multiplier 7.5 --spp 16 --seed 0`.
   - Compute the image mean; it must be **≈ 0.3214** (the banked 3090 value — images are GPU-independent).
3. Render the bunny too (`asset_validation`, bunny PLY, 512², meadow) → confirm it runs, **zero cap
   overflows**.
4. **Gate:** if the mean doesn't match → stop and debug the port. Do not proceed to SER on a broken build.

---

## Phase 2 — implement SER (minimal, compile-gated)
1. Add a `THESIS_ENABLE_SER` build option mirroring `THESIS_ENABLE_FAST_ERF` (define the device macro in
   `cmake/OptiX-IR.cmake`).
2. In `device/entry/raygen.cuh`, inside the bounce loop, **immediately after** `sample_scattering_event(...)`
   returns (before the shading / NEE / next-trace block), insert:
   ```cpp
   #ifdef THESIS_ENABLE_SER
       // Regroup threads by where they scattered, so the subsequent albedo/NEE/transmittance
       // traces run coherently. ScatteringEvent has no primitive id, so hash the quantised
       // scatter point (escape => 0). 8 bits is plenty for the scheduler.
       unsigned int ser_hint = 0u;
       if (result) {
           const float inv_cell = 8.0f;                 // ~scene-scale; tune to a few cells/axis
           const int cx = static_cast<int>(event.position_.x * inv_cell);
           const int cy = static_cast<int>(event.position_.y * inv_cell);
           const int cz = static_cast<int>(event.position_.z * inv_cell);
           ser_hint = (random::hash(cx * 73856093 ^ cy * 19349663 ^ cz * 83492791)) & 0xFFu;
       }
       optixReorder(ser_hint, 8);
   #endif
   ```
   - `optixReorder(unsigned coherenceHint, unsigned numCoherenceHintBitsFromLSB)` — confirmed signature in
     `optix_device.h`.
   - It is callable **only from raygen** (✓ — the loop is in `__raygen__rg`) and **may be called under
     divergent control flow** (threads that already `break`-ed simply don't participate).
   - Keep it **one** reorder per bounce. Don't also reorder before the shadow-ray trace in v1.
3. (Fallback hints if the spatial hash shows nothing: the no-arg `optixReorder()` (regroup by current
   scheduling/hit state), or a 1-bit scatter-vs-escape hint.)

---

## Phase 3 — A/B measurement
1. Build two binaries: **SER-off** (stock) and **SER-on** (`THESIS_ENABLE_SER`).
2. **Correctness first — SER must not change the image** (it is pure scheduling): render cloud SER-on vs
   SER-off, confirm **RMSE ≈ 0** (bit-identical up to FP reassociation). A changed image = a bug, not SER.
3. **Performance A/B** with `ncu` (it locks clocks itself → cap-immune and comparable), cloud + bunny, the
   §8.28 recipe (`--kernel-name "regex:optixLaunch" --launch-count 1 --section Occupancy
   --section SchedulerStats --section WarpStateStats --section SpeedOfLight`). Capture for each arm:
   - achieved occupancy, **eligible warps / scheduler**, **no-eligible (scheduler-idle) %**, SOL SM%,
     **avg active threads / warp**, registers/thread.
   - 3090 baselines to beat: cloud 31% occ / 53% idle / ~7 lanes; bunny 21% occ / 70% idle / ~5.4 lanes.
4. **Wall-clock** frame time, median of N, SER-on vs SER-off (locked clocks via `lock_clocks.sh` or just
   the ncu duration metric).
5. Record everything in `results/campaign/ser_ab.md` (+ raw ncu CSVs).

**What "it worked" looks like:** SER-on shows higher eligible-warps + SM%, lower scheduler-idle, more
active lanes/warp, and a frame-time drop — concentrated on the bunny (the more divergent asset).

---

## Phase 4 — fold into the thesis
- Compute SER speedup = (SER-off frame time) / (SER-on) on cloud + bunny.
- Rewrite the §6 SER autopsy from "cannot be brought to bear" to the measured outcome (helps X% /
  negligible), explicitly framed as a **cross-architecture probe on an Ada/Blackwell rental**, with all
  3090 numbers unchanged. A one-line forward pointer from §7 (limitations/future) is enough.
- If SER helps materially, this also retroactively **validates the latency/divergence diagnosis** of
  §6.1 — worth a sentence there.

---

## Risks / notes
- **Port friction is the time sink**, not the experiment (OptiX SDK + driver + assets + build). The
  measurement itself is minutes; keep the rental short.
- SER in a megakernel-with-loop is less battle-tested than in a wavefront raygen. If one-reorder-per-bounce
  shows nothing, try: reorder only at bounce 0 (the most divergent first trace), or reorder before the
  shadow-ray trace instead.
- `ScatteringEvent` (`include/thesis/device/optix/scattering_event.h`) has no primitive id — hence the
  spatial-hash hint. If a cheap winning-prim id can be threaded out of `sample_scattering_event`
  (`device/core/sampling.cuh`), it is a slightly better coherence key, but not required for v1.
- Confirm the image is unchanged (Phase 3.2) before believing any speedup — a "faster" SER build that
  changed the image is just broken.

## Verification checklist
- [ ] GPU is Ada/Blackwell (CC 8.9 or 12.0), driver OK, OptiX 9 builds.
- [ ] Baseline cloud mean ≈ 0.3214 (port correct).
- [ ] SER-on image == SER-off image (RMSE ≈ 0).
- [ ] ncu deltas reproducible across repeats; frame-time delta consistent with the occupancy/eligible-warp
      deltas.
- [ ] `results/campaign/ser_ab.md` written; §6 updated; 3090 numbers untouched.
