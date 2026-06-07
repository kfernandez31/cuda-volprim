# Wavefront Path-Tracer Rewrite — Plan

**Status:** proposal, not started. **Supersedes** `hybrid-wavefront-plan.md` (stale — see §3).
**Author basis:** FINDINGS §8.28–§8.33 + fresh profiling (this doc, 2026-06-07, RTX 3090, current
`main`-equivalent build with RR=12).

> **One-line thesis:** the renderer is correct and *beats* Mitsuba on quality (§8.11), but is stuck at
> ~22 % GPU occupancy in a single 114-register megakernel, latency-bound on scattered global
> `Primitive` loads. Every cheap lever has been tried and exhausted (§8.29–§8.33). The wavefront split
> is the one remaining structural lever — and it simultaneously unlocks **three** otherwise-parked wins
> (occupancy, path splitting, adaptive sampling). It is a real bet, not a slam-dunk; **Phase 1 is the
> go/no-go gate.**

---

## 1. Goal

Raise GPU occupancy by splitting the monolithic megakernel into several smaller-register kernels, so the
scheduler has enough warps to hide the dominant memory-latency stall. Target: **per-spp throughput**
(the ~1.93× half of the Mitsuba gap, §8.5). It does **not** touch correctness or the per-sample variance
(already addressed: NEE+MIS, analytic transmittance, RR=12).

## 2. Measured baseline (the anchor — re-profiled on the current build)

**Render kernel `optixLaunch` @ asset_validation 256², 16 spp (matches §8.28):**

| Metric | Value | Reading |
|---|---|---|
| Registers/thread | **114** | the occupancy limiter |
| Achieved occupancy | **23.7 %** | vs ~37 % register-ceiling; scheduler starved |
| Stall: long_scoreboard | **4.28** | DOMINANT — global memory latency (scattered `Primitive` loads) |
| Stall: wait (arith dep) | **2.00** | the erf chains |
| DRAM throughput | **~1 %** | huge bandwidth headroom (matters for added RayState traffic) |

**Time-split (cloud cam0, σ=7.5, albedo 0.9, 64 spp; ±1 s granularity — directional, not precise):**

| Configuration | Time | Δ | What it isolates |
|---|---|---|---|
| Trace + scatter, **NEE off** | **4 s** | — | immovable OptiX core (primary trace + argmin + escape) ≈ **44 %** |
| **NEE on, MIS off** (1 shadow ray) | 7 s | +3 s | first NEE shadow-ray transmittance integration |
| **NEE + MIS** (2 shadow rays) | 9 s | +2 s | MIS second shadow ray |

→ **~44 % primary core, ~56 % shadow-ray integration.** Note this is the *first time* this split was
measured on the optimized build; it is **very different from the old plan's 15 %/85 %** (anyhit fusion
§8.18 shrank the shadow work; RR=12 grew the core). Implication: the wavefront's headroom is **smaller
than the old doc assumed** — be sober about expected speedup (§9).

**Key architectural fact:** *both* the primary trace AND the NEE/MIS shadow rays are `optixTrace` calls
(they need BVH traversal on the RT cores). So "move the work to a pure CUDA kernel" — the old plan's
framing — is **wrong**. The wavefront win here is **not** relocating work off OptiX; it is **splitting
the one 114-reg kernel into several low-reg kernels**, each of which runs at higher occupancy, so the
global-`Primitive`-load latency (our bottleneck) is hidden by more in-flight warps.

## 3. Why the old `hybrid-wavefront-plan.md` is superseded

It was written on `feature/pre-nee` (commit ad1c586), before NEE/MIS and the analytic-transmittance
architecture. Specifically wrong now:
- It targets extracting the **escape path** (the old segment-marching + sort + solver). That code is
  **gone** — the current escape is trivial (argmin returns "no scatter" → env). Nothing to extract.
- Its time-split (15 % core / 85 % escape) is stale; the real split is ~44/56 and the heavy part is the
  **NEE/MIS shadow-ray transmittance**, not escape.
- Its RayState struct lists `BitVector<256>` for active_prims; current build uses `CompactSet` (>256
  prims). Sizes below are corrected.

The phasing philosophy (global RayState first as a gate, then split, measure, fall back) is still right
and is carried forward.

## 4. Target architecture

Replace the single in-raygen bounce loop with a **host-driven bounce loop** over a queue of live
path-states, with per-bounce kernels:

```
Host, per frame:
  init RayState[N] for all pixels×spp-batch ; alive_count = N
  for bounce in 0..MAX_BOUNCES:
      if alive_count == 0: break
      optixLaunch  TRACE     (active_queue)      // primary trace + argmin scatter  → hit/scatter/escape
      cudaLaunch   SHADE     (active_queue)      // albedo, phase sample, RR, build NEE requests, next ray
      optixLaunch  CONNECT   (nee_queue)         // NEE/MIS shadow rays → analytic transmittance → radiance
      compact      active_queue                  // drop dead/escaped/RR-killed; (later) enqueue splits
  cudaLaunch finalize (Welford mean → image)
```

- **TRACE** (OptiX): per active ray, `optixTrace` collect-mode + argmin (the existing
  `sample_scattering_event`). Writes scatter point / escape flag to RayState. *Fewer registers than the
  megakernel* because it no longer also contains shading + NEE + MIS + Welford.
- **SHADE** (CUDA, high-occupancy): reads scatter points, does the heavy **global `Primitive` loads** for
  `evaluate_albedo` + phase sampling + throughput + RR, emits the next ray and the NEE/MIS direction
  requests. *This is the kernel that benefits most from occupancy* — it's where the dominant
  long_scoreboard load latency lives, and as a small CUDA kernel it can hit 50–70 % occupancy.
- **CONNECT** (OptiX): the NEE/MIS shadow rays (transmittance via the fused transmittance-anyhit, §8.18).
  Stays OptiX (needs traversal) but as its own small kernel runs at higher occupancy than when inlined.
- **COMPACT** (CUDA/CUB): stream-compact the live-ray indices so TRACE/SHADE stay dense as paths die —
  this is what fixes the divergence that caps the megakernel (§8.28: 20.5/32 active lanes) and what
  adaptive sampling needed (§8.30).

### Data structures (global memory)

```cpp
struct RayState {                 // ~140–170 B (CompactSet active_prims dominates)
    float3 origin, direction;     // 24 B   current ray
    float3 throughput, radiance;  // 24 B
    random::PCG32 rng;            // 16 B
    float3 mean, M2;              // 24 B   Welford (M2 only if adaptive on)
    PrimsSet active_prims;        // ~264 B if CompactSet<128>  ← the heavy field (see Risk R1)
    uint32_t pixel_idx, sample_count;
    uint16_t bounce; uint8_t flags;  // alive / escaped / scattered
};
uint32_t* active_queue;  uint32_t* alive_count;   // compacted live-ray indices
struct NeeRequest { uint32_t ray_idx; float3 dir; float3 weight; }; NeeRequest* nee_queue;
```

For 1920×1080 at one sample in flight: ~150 B × 2.07 M = **~310 MB** (fits 24 GB easily). At 900×600
(cloud cam): ~80 MB. **The `active_prims` CompactSet (~264 B) dwarfs everything else** → see R1.

## 5. Phased implementation — each phase validated and reversible

> Validation after EVERY phase: **furnace** (energy flat, the bias gate) + **systematic vs Mitsuba ≤1e-4**
> (the refactor is correctness-preserving, so any diff = a bug) + a few cloud cams. Keep `main` shippable.

### Phase 0 — Gate / profile ✅ (this document)
Occupancy 23.7 %, scheduler starved, time-split ~44/56, DRAM headroom confirmed. **Go for Phase 1.**

### Phase 1 — Global RayState + host bounce loop (NO kernel split) — **THE GO/NO-GO GATE** — ~1.5–2 d
Move per-bounce state (ray, throughput, radiance, rng, active_prims, Welford) from raygen locals into a
global `RayState[N]`; convert the in-raygen sample loop into a host loop of `optixLaunch` calls (one per
bounce), each reading/writing RayState. **No compaction, no split yet** — every pixel's thread runs every
bounce launch (dead ones early-out).
- **Purpose:** measure the *cost of streaming RayState to global memory every bounce* — on the exact
  memory-latency axis we're already bound on (the #1 risk). This is the cheapest possible probe of the
  whole bet.
- **Decision:** if Phase 1 is within ~10–15 % of the megakernel → the global-traffic tax is affordable,
  proceed. If it's catastrophic (≥1.5×) → **STOP**; the megakernel is the defensible final state
  (document the global-traffic wall as the finding). This de-risks the entire rewrite for <2 days.

### Phase 2 — Stream compaction of the active-ray queue — ~1.5 d
Add `active_queue` + `alive_count`; after each bounce, compact (CUB `DeviceSelect`) so launches size to
live rays only. This is what converts the global-RayState overhead into a *win*: dead/escaped/RR-killed
paths stop consuming launch slots → directly attacks the divergence ceiling (§8.28). Measure occupancy +
wall-clock vs Phase 1.

### Phase 3 — Split SHADE into a CUDA kernel — ~2 d
Extract albedo/phase/RR/next-ray + NEE-request generation from the OptiX raygen into a high-occupancy
CUDA kernel reading the compacted queue. **This is where the occupancy win is meant to land** (the heavy
`Primitive` loads run at 50–70 % occupancy instead of 22 %). Re-profile: expect the TRACE kernel's regs to
drop (no shading inlined) and SHADE to hide the long_scoreboard latency. If neither moves → the JIT was
already spilling, not the structure (Risk R3) → reassess.

### Phase 4 — Separate CONNECT (NEE/MIS shadow rays) optixLaunch — ~1.5 d
Move the NEE/MIS shadow rays into their own small optixLaunch over `nee_queue`. The ~56 % shadow
integration now runs at its own (higher) occupancy. Measure.

### Phase 5 — Path splitting (the parked variance lever) — ~1–1.5 d
Now trivial on the queue: at a high-throughput / shallow vertex, SHADE pushes **N copies** of the
path-state (each `throughput/N`, decorrelated rng) to the next-bounce queue instead of 1. This is the
unbiased variance reducer for the multiple-scattering tail that the single-path megakernel structurally
could not do (§8.33). Gate splitting on depth/throughput so it doesn't explode the queue. Validate
furnace + systematic; measure equal-quality time.

## 6. What the wavefront unlocks (why it's worth the cost)

The queue/compaction architecture is a **common enabler** for three separate wins, two of which we already
proved are otherwise impossible here:
1. **Occupancy** — the direct goal (per-spp throughput).
2. **Path splitting** (§8.33, Phase 5) — needs a per-path queue the megakernel lacks.
3. **Adaptive sampling** (§8.30) — failed in the megakernel *only* because per-pixel early-return doesn't
   free a warp until all 32 lanes converge; **compaction removes converged rays from the queue
   regardless of warp coherence**, which is exactly the mechanism §8.30 said it needed. Re-enabling
   adaptive on top of the wavefront is nearly free.

This is the case for doing the wavefront over any single micro-lever: it's the one change that pays back
on three axes. (It is also the *software* substitute for OptiX SER, which would do the divergence fix in
hardware but is **Ada-only** — N/A on this Ampere 3090, §8.28.)

## 7. Risks & fallback

| # | Risk | Mitigation / test |
|---|---|---|
| **R1** | **RayState global traffic on the bottleneck axis.** Streaming ~150 B/ray/bounce (esp. the ~264 B `active_prims` CompactSet) could add more memory latency than occupancy hides. | Phase 1 measures it directly. *Shrink active_prims* (don't persist it — rebuild at each scatter, or store only a compact prim-id list). DRAM is at 1 % so bandwidth isn't the worry; *latency* is — compaction (Phase 2) + coalesced SoA RayState mitigate. |
| **R2** | **44 % immovable core limits the ceiling.** Unlike the old 85 %-movable assumption, nearly half the time is primary trace that the split helps less. | Honest expectation-setting (§9). The win comes from occupancy on *all* kernels, not relocating the core; but the upside is capped — Phase 3 measurement decides if it's worth Phases 4–5. |
| **R3** | **OptiX JIT still allocates ~114 regs** in the trace kernel even with shading removed → occupancy unchanged. | Phase 3 PTX/ncu check. If so, the split still cuts LMEM spill traffic (smaller win) but not occupancy → fall back. |
| **R4** | **Compaction + N launches/bounce overhead** eats the gain on scenes with short paths. | Measure across escape/scatter ratios; keep the megakernel as a compile-time-selectable path. |

**Fallback (any phase):** the megakernel at 22 % occupancy with all current optimizations + RR=12 is a
**defensible, shippable final state** that already beats Mitsuba on quality. Document the occupancy
ceiling + the chosen phase's measurement as the finding. `main` stays on the megakernel until the
wavefront *measurably* wins end-to-end.

## 8. Expected outcomes (honest, given the 44 % core)

| Metric | Megakernel (now) | Wavefront (target) | Confidence |
|---|---|---|---|
| TRACE kernel regs | 114 | ~70–90 | medium |
| SHADE occupancy | 23.7 % (whole) | 50–70 % | medium |
| Per-spp wall-clock | 1.0× | **0.65–0.85×** (≈1.2–1.5×) | **low-medium** |
| Equal-quality (with Phase 5 splitting) | 1.0× | further ~1.1–1.3× | low |

The old doc guessed ~1.7–2×; with the *measured* 44 % immovable core and the RayState traffic tax, **a
realistic target is ~1.2–1.5× per-spp**, decided empirically at Phases 1 and 3. Anything more is upside.

## 9. Effort & timeline

| Phase | Effort | Gate |
|---|---|---|
| 1 — global RayState (gate) | 1.5–2 d | **stop if global traffic catastrophic** |
| 2 — compaction | 1.5 d | occupancy must rise |
| 3 — SHADE CUDA kernel | 2 d | the occupancy payoff must appear |
| 4 — CONNECT optixLaunch | 1.5 d | — |
| 5 — splitting | 1–1.5 d | unbiased + equal-quality win |
| **Total** | **~7–8 d** | each phase reversible; main shippable throughout |

The irreducible cost is **GPU validate-debug latency**, not code authoring: each phase is many
furnace+systematic renders (minutes each) plus iterate-render-compare loops on a correctness-critical
refactor. Estimate assumes the RTX 3090 + the existing validation harness (`validate_ladder.sh`,
`sg_systematic.py`, `furnace_check.py`, `exr_diff.py`/`exr_rmse.py`).

## 10. Validation strategy (reused, do not reinvent)

- **Per phase:** furnace (energy flat) + multi-seed systematic vs Mitsuba-analog ≤~1e-4 + a few cloud cams
  bit-diff vs the megakernel baseline. The refactor is correctness-preserving → any systematic diff = bug.
- **Phase 5 (splitting):** additionally furnace + systematic (splitting must stay unbiased) + measured
  equal-quality time (RMSE²·time vs the megakernel, like §8.33).
- **Perf:** ncu (occupancy, regs, long_scoreboard) per kernel + wall-clock across escape/scatter-ratio
  scenes (thin single-Gaussian → dense cloud → stress_8192).

---

### Appendix: profiling commands used to derive this plan
```bash
# Occupancy / regs / stalls (render kernel), matches §8.28:
SG_RES=256 ncu -k "regex:optixLaunch" -c 1 \
  --metrics launch__registers_per_thread,sm__warps_active.avg.pct_of_peak_sustained_active,\
smsp__average_warps_issue_stalled_long_scoreboard_per_issue_active.ratio,\
smsp__average_warps_issue_stalled_wait_per_issue_active.ratio \
  test_runner --scene asset_validation --spp 16
# Time-split: rebuild with ENABLE_NEE / ENABLE_MIS toggled in device/core/constants.cuh,
# render cloud_asset_scattering cam0 64spp, compare wall-clock (FULL vs MIS-off vs NEE-off).
```
