# Hybrid Wavefront Path Tracer — Findings & Implementation Plan

## Current State (feature/pre-nee, commit ad1c586)

### Architecture
Monolithic raygen kernel: one `optixLaunch` per frame handles tracing, hit collection, argmin scatter sampling, escape integration, shading, and Welford accumulation. All per-bounce logic lives in `__raygen__rg()` with `sample_scattering_event` marked `__noinline__` (ignored by OptiX JIT).

### Performance Profile (stress_8192_gaussians, 32spp, RTX 3090)

| Metric | Value |
|--------|-------|
| Render time | 2.65s |
| Registers/thread | 114 |
| Occupancy | 32.7% |
| LMEM depot | 3248 bytes |
| LMEM load sectors | 31M |
| LMEM store sectors | 62M |
| Theoretical max occupancy (at 114 regs) | 35.4% |

### Optimizations Applied
1. **Workspace sharing** — hit_buffer and events passed as parameters, not re-allocated per call
2. **Cached exits elimination** — exits recomputed on demand in escape and scatter paths
3. **HIT_BUFFER_CAPACITY decoupled** from MAX_PRIMITIVES (128 vs 256)
4. **`__noinline__` on sample_scattering_event** — JIT re-inlines but cleaner PTX gave ~24% speedup
5. **PCG32 RNG** replacing curandState (80B → 16B) — ~19% speedup, LMEM traffic halved

### Root Cause of 32% Occupancy Ceiling
The JIT register allocator sees the combined live-variable pressure of all code paths inlined into one function. Forcing lower register counts (`--maxrregcount` sweep: 80, 96, 128) trades registers for LMEM spills — a wash when LMEM traffic is already the bottleneck. To use fewer registers without more spills, the kernel must have fewer simultaneously-live values.

---

## Proposed Hybrid Wavefront

### Motivation
The escape path is the heaviest code in the raygen kernel:
- Allocates EventBuffer (`StaticVector<HitRecord, 256>` = 2056 bytes)
- Copies active_prims into `current_active`
- Recomputes exits for all active + hit-buffer primitives
- Sorts events by t-value
- Iterates segments, integrating optical depth per primitive (erf math)

Extracting this to a separate CUDA kernel removes ~60% of the raygen's code complexity while keeping the common path (trace + scatter) monolithic and fast.

### Architecture

```
Host bounce loop (per frame):
    for bounce in 0..MAX_BOUNCES:
        escape_count = 0
        optixLaunch(raygen, width, height)           // trace + scatter
        if escape_count > 0:
            cuda_launch(process_escapes, escape_count)  // escape integration
        if no rays alive: break
    cuda_launch(finalize_image)                      // write Welford stats
```

### Data Structures (global memory)

```cpp
// Per-ray persistent state across bounces
struct RayState {
    float3 origin;           // 12B
    float3 direction;        // 12B
    float3 throughput;       // 12B
    float3 radiance;         // 12B
    random::PCG32 rng;       // 16B
    PrimsSet active_prims;   // 40B  (BitVector<256>)
    float3 mean;             // 12B  (Welford)
    float3 M2;               // 12B  (Welford)
    uint32_t sample_count;   //  4B
    uint32_t pixel_idx;      //  4B
    uint8_t bounce;          //  1B
    bool alive;              //  1B
};                           // ~138B per ray → ~276MB for 1920x1080
                             // (fits comfortably in 24GB VRAM)

// Escape queue entry
struct EscapeEntry {
    uint32_t ray_idx;        // index into RayState array
    float3 env_color;        // from miss shader (captured at escape time)
};

// Device-side queue with atomic counter
EscapeEntry* escape_queue;   // [WIDTH * HEIGHT]
uint32_t* escape_count;      // atomic counter, reset per bounce
uint32_t* alive_count;       // tracks active rays for early termination
```

### Kernel 1: Raygen (optixLaunch)

Responsibilities:
- Read ray state from global memory
- Run optixTrace in anyhit-collect mode (unchanged)
- Argmin scatter sampling over active_prims + hit_buffer (unchanged)
- **If scatter**: rebuild active_prims, evaluate albedo, sample phase, update throughput, Russian roulette, write new ray to state
- **If escape**: enqueue to escape_queue, mark ray dead

What's removed vs current raygen:
- EventBuffer allocation and population
- Exit recomputation for escape path
- Event sorting
- Segment-by-segment optical depth integration
- PrimsSet current_active copy

Expected register reduction: 114 → ~70-80 (estimate based on removed code complexity).

### Kernel 2: Process Escapes (CUDA kernel)

```cpp
__launch_bounds__(256, 4)  // target: 64 regs, 4 blocks/SM → 66% occupancy
__global__ void process_escapes(
    RayState* ray_states,
    const EscapeEntry* escape_queue,
    uint32_t escape_count,
    const Primitive* primitives,
    /* ... */
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= escape_count) return;

    auto entry = escape_queue[idx];
    auto& state = ray_states[entry.ray_idx];

    // Reconstruct ray
    auto ray = Ray(state.origin, state.direction);
    auto& active_prims = state.active_prims;

    // ── Escape path (moved from raygen) ──
    // 1. Re-trace to get hit_buffer (entry hits)
    //    Option A: store hit_buffer in global memory from raygen
    //    Option B: re-trace here (costs one extra optixTrace equivalent)
    //    → Option A preferred: avoid redundant traversal

    // 2. Build events from active_prims exits + hit_buffer entries/exits
    // 3. Sort events
    // 4. Segment-by-segment optical depth integration
    // 5. Accumulate: state.radiance += state.throughput * exp(-tau) * entry.env_color

    state.active_prims.clear();
    state.alive = false;
}
```

### Open Design Decision: Hit Buffer in Escape Kernel

The escape kernel needs the hit_buffer (entry hits from the trace). Two options:

**Option A — Persist hit_buffer from raygen to global memory:**
- Raygen writes hit_buffer to a per-ray global allocation after tracing
- Escape kernel reads it back
- Cost: ~1KB extra global memory write/read per escaped ray
- Pro: no redundant traversal

**Option B — Re-trace in the escape kernel:**
- Escape kernel calls optixTrace again to re-collect hits
- Cost: redundant BVH traversal
- Pro: no extra memory; escape kernel is self-contained
- Con: escape kernel must be an optixLaunch, not a pure CUDA kernel

**Recommendation:** Option A. The hit_buffer is small (~1KB) and already in cache from the raygen trace. Writing it to global memory is cheap compared to re-traversing the BVH. This lets the escape kernel be a pure CUDA kernel with `__launch_bounds__` control.

This means the raygen must write hit_buffer to global memory for ALL rays (not just escapes), since we don't know at trace time whether the ray will escape. Alternatively, use a two-phase approach within raygen: trace → argmin → if escape, write hit_buffer then enqueue.

Since hit_buffer is only needed on escape, and we know escape vs scatter after the argmin loop, we can write hit_buffer to global memory only for escaped rays:

```cpp
// In raygen, after argmin:
if (t_scatter_min >= INF_F) {
    // Write hit_buffer to global memory (only for escapes)
    memcpy(&global_hit_buffers[ray_idx], &hit_buffer, hit_buffer.size_bytes());
    uint32_t idx = atomicAdd(escape_count, 1);
    escape_queue[idx] = { ray_idx, env_color };
    state.alive = false;
    return;
}
```

### Implementation Steps

#### Phase 1: Global ray state (no wavefront yet)
1. Define `RayState` struct
2. Allocate `RayState[width * height]` in renderer setup
3. Move raygen's local variables (throughput, radiance, rng, active_prims) to read/write from `RayState`
4. Move the sample loop from raygen to a host-side bounce loop (multiple optixLaunch calls)
5. Verify: renders match current output, measure performance delta

This phase isolates the cost of global memory ray state without any architectural change. If the overhead is catastrophic, we stop here.

#### Phase 2: Extract escape path
1. Allocate `EscapeEntry[width * height]` + atomic counter
2. Allocate `HitBuffer[width * height]` for persisting hits on escape
3. In raygen: after argmin, if escape → write hit_buffer + enqueue, return
4. Write `process_escapes` CUDA kernel with `__launch_bounds__`
5. Host bounce loop: optixLaunch → process_escapes → repeat
6. Verify: renders match, measure register count and occupancy for both kernels

#### Phase 3: Measure and tune
1. PTX analysis on new raygen (expect ~70-80 regs)
2. ncu profiling: occupancy, LMEM traffic (expect near-zero for raygen)
3. Tune `__launch_bounds__` on escape kernel
4. Compare wall-clock time against monolithic baseline
5. Test with multiple scenes (varying escape/scatter ratio)

### Expected Outcomes

| Metric | Current | Expected (hybrid) |
|--------|---------|-------------------|
| Raygen registers | 114 | ~70-80 |
| Raygen occupancy | 32% | ~50-60% |
| Raygen LMEM | 3248B | ~1000B (hit_buffer only) |
| Escape kernel registers | N/A | ~50-60 |
| Escape kernel occupancy | N/A | ~60-80% |
| Global memory overhead | 0 | ~276MB (ray state) |
| Kernel launches per bounce | 1 | 2 |
| Wall-clock (estimate) | 2.65s | 1.5-2.0s (speculative) |

### Risks

1. **Global memory overhead kills the gain.** Reading/writing 138B of ray state per bounce per ray is ~2GB of traffic over 8 bounces. If L2 can't absorb this, we lose more than we gain from occupancy. Phase 1 measures this directly.

2. **Escape path isn't the bottleneck.** If most bounces scatter (not escape), the raygen still dominates and the escape kernel runs on few rays. The register reduction in raygen is the real win; the escape kernel is secondary.

3. **OptiX JIT still uses 114 registers.** Even with less code in raygen, the JIT might allocate the same register count and just spill less. This would still help (less LMEM traffic) but wouldn't increase occupancy. Phase 2 measures this.

4. **Hit buffer persistence adds complexity.** Writing hit_buffer to global memory only on escape requires the buffer to survive until the escape kernel runs. Memory lifetime management adds code complexity.

### Fallback

If hybrid wavefront doesn't deliver meaningful speedup after Phase 1, the current monolithic architecture at 32% occupancy with PCG32 + LMEM optimizations is a defensible final state. Document the occupancy ceiling and its root cause (OptiX JIT register allocation in monolithic kernels) as a finding.
