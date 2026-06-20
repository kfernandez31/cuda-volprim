# Device-code review — handoff for a focused code-review session (2026-06-17)

Scope: correctness/efficiency/readability of the OptiX/CUDA device code. **Not** the thesis.
Each item below is verified against the source at the cited `file:line`. Verdicts are the author's
current best read; the reviewer should confirm and decide.

## CRITICAL CONTEXT — read first
The render kernel is **latency-bound**, not compute-bound: NSight Compute puts it at ~31% SM / ~16.5%
DRAM throughput on the cloud (Ch6 `sec:bottleneck`, `results/campaign/ncu_summary.md`), dominated by
scattered global loads of the per-primitive `Primitive` struct and by register pressure / occupancy
(~21–31%). **Implication:** arithmetic micro-optimisations (FMA, min/max, fewer flops) will *not* move
frame time — the math is already hidden behind memory stalls. Real wins are in **memory access pattern,
occupancy, register pressure, and wasted work**. Prioritise accordingly; treat the compute-cleanliness
items as readability/precision, not speed.

## Items

### 1. `device/entry/anyhit.cuh:38` — possibly-redundant back-face check
`if (optixIsTriangleBackFaceHit()) { optixIgnoreIntersection(); return; }`, inside
`#ifdef THESIS_ICOSPHERE`. It exists only in the tessellated-icosphere A/B build (Ch6), to keep one
entry hit per convex primitive; the production build uses built-in spheres (one entry natively) and
never compiles this. **Question to resolve:** the trace already sets
`OPTIX_RAY_FLAG_CULL_BACK_FACING_TRIANGLES` (`device/core/trace.cuh`), and the icosphere winding is
outward-CCW (comment) — so OptiX should cull back faces *before* any-hit, making this check dead code.
**Action:** in the icosphere build, remove the check and confirm the Ch6 icosphere A/B is unchanged
(no double-counting). If it *does* change, the cull flag isn't culling for this GAS (winding/flag
subtlety) — keep the check and instead drop the now-redundant cull flag for that build, and document
which mechanism is load-bearing. Either way one of the two is redundant.

### 2. `device/core/sampling.cuh:89` — `onb_from_normal` FMA opportunities
The two diagonal terms are plain mul-adds: `1.0f + sign*n.x*n.x*a` (→ `fmaf(sign*n.x*n.x, a, 1.0f)`)
and `sign + n.y*n.y*a` (→ `fmaf(n.y*n.y, a, sign)`). The `sample()` function just below already uses
`math::fma` throughout (lines 104–112), so this helper simply predates / missed that pass.
**Action:** apply `math::fma` for consistency + precision. **Caveat:** latency-bound kernel — expect
*no* frame-time change; this is cleanliness, not speed.

### 3. `device/entry/raygen.cuh:122–130` — AOV capture not gated by denoiser (REAL wasted work)
The AOV *reads* are null-gated (`if (launch_params.image_.albedo_aov_)`, line 45), but the per-sample
*capture* is not: at `bounce == 0`, `sample_aov_albedo = evaluate_albedo(event.position_,
event.active_prims_)` runs every primary sample regardless of `--denoise`. `evaluate_albedo` is a
real σ_t-weighted mixture scan over the active set, so when the denoiser is off (the default, and what
all perf/validation runs use) this is pure waste. **Action:** gate the bounce-0 AOV capture (and its
Welford write-back further down) behind the same `albedo_aov_ != nullptr` check used at line 45, so it
compiles away to nothing when AOVs aren't allocated. Verify denoiser-on output is byte-identical and
denoiser-off frame time drops slightly. This is the one item here that can actually help frame time.

### 4. min/max — use the existing helpers, no new macro needed
`math::min` / `math::max` already exist and are used (e.g. `sampling.cuh:115`). The author saw no raw
ternary min/max in `sampling.cuh`/`raygen.cuh`, but the reviewer should sweep the whole `device/` tree
for raw `(a<b?a:b)` / open-coded `fminf`/`fmaxf` and replace with the helpers for consistency. A new
`MIN`/`MAX` macro is *not* wanted (macros over the existing typed helpers are a regression).

### 5. `device/entry/raygen.cuh` — megakernel readability refactor
The ray-generation program holds the whole bounce loop and is hard to read. Refactor into named
`__device__ __forceinline__` helpers — e.g. `capture_aov_bounce0(...)`, `analytic_direct_term(...)`,
`nee_mis_direct(...)`, `russian_roulette(...)`, `welford_accumulate(...)` — **behaviour-preserving**.
**Hard constraint:** this is a megakernel whose performance depends on everything staying inlined in
one kernel with state in registers (Ch4 `sec:gpu-impl`, Ch6 wavefront autopsy). Every helper must be
`__forceinline__`, must not spill the active set / hit buffer to global memory, and the refactor must
be validated frame-time-neutral (interleaved A/B at locked clocks) and output-identical. Readability
only — zero behaviour or perf change is the acceptance bar.

### 6. General device-inefficiency pass — profile-guided, memory-first
Given the latency-bound profile (top of doc), audit for: redundant global loads of `Primitive` fields
(can they be loaded once / cached in registers across the bounce?); `__ldg`/read-only path coverage on
all read-only primitive data; SoA access coalescing on the hit buffer; register pressure (114 regs/
thread per `ncu_summary.md` — anything that lowers it raises occupancy); and wasted work like item 3.
Use the §8.28 ncu recipe (`results/campaign/ncu_summary.md` has the current numbers to beat). Do **not**
chase flop-count reductions — they won't show up. Every candidate change must be validated
output-identical (or within the documented FMA-reorder tolerance) and frame-time A/B'd at locked clocks
(`scripts/campaign/lock_clocks.sh`; the ~3× ambient-state swing at the un-pinned operating point means
small effects need interleaved A/B — see `results/campaign/caps_ab.md`).

## Validation harness the reviewer should use
- Correctness gate: furnace (`single_gaussian_validation`, albedo 1, constant env, 1024 spp →
  `tools/refs/furnace_check.py`) + the absorption ladder; both must still pass.
- Frame-time A/B: locked clocks + interleaved rounds (see `caps_ab.md` / `caps_timing.md` for the
  pattern); never trust a single un-interleaved timing.
- Build: `cmake --build build --target test_runner -j` (~9 s incremental).
