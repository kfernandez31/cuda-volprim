Optimizations done:
- Bit vector set (64-bit words, `__ffsll`, cached size)
- Adaptive sorting dispatcher (insertion sort ≤64, bitonic for power-of-2, warp shuffle ≤32)
- Morton 3D coding
- No sorting + argmin (ADT scatter eliminates sort in scatter case)
- Unified hit record (12 bytes down from 20)
- `__ldg` read-only cache hints (Welford state loads)
- CUDA `tex2D` for environment map access
- `float4` aligned buffers (128-bit coalesced access)
- Interleaved sphere data (center xyz + radius in float4)
- Grid-stride loops for coalesced access
- Pinned host memory (`cudaHostAlloc`) for DMA transfers
- Async memory alloc/free (`cudaMallocAsync`/`cudaFreeAsync`)
- Fast math flags (`--use_fast_math`, `--fmad=true`, `--ftz=true`, `--prec-div=false`, `--prec-sqrt=false`)
- CUDA intrinsics (`__fsqrt_rn`, `__frcp_rn`, `__expf`, `__logf`, `__sinf`, `__cosf`, `rsqrtf`)
- FMA chains (`fmaf` for dot products, cross products, lerp)
- `#pragma unroll 4` on primitive iteration loops
- `__device__ __forceinline__` throughout
- OptiX optimization levels (`OPTIX_BUILD_FLAG_PREFER_FAST_TRACE`)
- GAS compaction (`OPTIX_BUILD_FLAG_ALLOW_COMPACTION`)
- Buffered anyhit + no closesthit
- Backface culling (`OPTIX_RAY_FLAG_CULL_BACK_FACING_TRIANGLES`) — collect only entries
- Builtin sphere intersections
- OptiX IR modules
- Adaptive sampling (per-pixel convergence, relative error threshold, min samples)
- Russian roulette termination (after depth 5, `RR_MAX_SURVIVAL = 0.99`)
- Batched rendering (multiple samples per launch)
- Batched online averaging (Welford's algorithm — single-pass mean + variance)
- Analytical optical depth (erf-based closed form)
- Analytical exit computation (direct formula, no quadratic solve)
- Camera-inside detection (precomputed on CPU, passed as buffer)
- Cached exit computations (reuse argmin exits in escape case)
- CUDA streams (6-stream DAG with event-based dependencies)
- Async CPU work (incl. file I/O, overlapped with GPU)
- Precompiled headers
- `cudaMemcpyAsync` for overlapped transfers

---

## Big-O Complexity Analysis

### Variables

| Symbol | Meaning |
|--------|---------|
| P | Total pixels (width × height) |
| S | Samples per pixel (spp) |
| N | Total scene primitives |
| A | Active primitives (overlapping at ray position), A ≤ 64 |
| H | Entry hits from BVH trace |
| E | Events in escape path (≤ 2H + 2A) |
| D | Path depth (bounces), D ≤ 128 |
| W | BitVector words (⌈N/64⌉) |

### Per-Step Complexity (current implementation)

| Step | Scatter (common) | Escape (terminal) | Notes |
|------|-------------------|--------------------|-------|
| Containment pre-populate | O(N) | O(N) | Linear scan over all primitives, BitVector insert O(1) each |
| BVH traversal (OptiX) | O(log N) | O(log N) | Hardware RT cores |
| Argmin inv_cdf sampling | O(A + H) | O(A + H) | One inv_cdf per primitive, no sorting needed |
| Exit computation | O(A + H) lazy | O(A + H) | Cached from argmin loop in escape case |
| Sorting | **None** | O(E²) or O(E log² E) | ADT eliminates sort for scatter; escape uses adaptive sort |
| Segment integration | **None** | O(E × A) | Only on escape: march sorted events, integrate per segment |
| Bisection search | **None** | N/A | Replaced entirely by inv_cdf |
| Active set rebuild | O(A + H) | N/A | Check cached exits against scatter point |
| Albedo evaluation | O(A) | N/A | Density-weighted average over active prims |
| Set insert/erase/contains | O(1) each | O(1) each | BitVector: bit OR/AND/test |
| Set iteration | O(W) per loop | O(W) per loop | __ffsll word scan, W = ⌈N/64⌉ |
| Env map lookup | O(1) | O(1) | tex2D hardware filtered |
| Welford accumulation | O(1) | O(1) | Online mean + variance |
| Adaptive convergence check | O(1) | O(1) | Skip pixel if variance below threshold |

### Per-Bounce Totals

**Scatter bounce (common path):**
```
O(N + A + H)    — linear in primitives touched
```

**Escape bounce (terminal, once per path):**
```
O(N + E × A)    — segment integration dominates
```

### Full Render

```
O(P_eff × S_eff × ((D-1) × (N + A) + (N + E·A)))
```

Where P_eff ≤ P (adaptive sampling skips converged pixels) and S_eff ≤ S (early stopping per pixel).

### Comparison vs. Pre-Optimization (segment-by-segment approach)

| Step | Before | After | Improvement |
|------|--------|-------|-------------|
| Per scatter bounce | O(N + A²) | O(N + A) | Quadratic → linear (ADT argmin) |
| Set insert/erase | O(A) | O(1) | BinarySet shift → BitVector bit op |
| Set contains | O(log A) | O(1) | Binary search → bit test |
| Sort (scatter path) | O(H²) or O(H log² H) | O(0) | Eliminated entirely |
| Segment integration (scatter) | O(H × A) | O(0) | Replaced by inv_cdf |
| Bisection solver | O(K × A), K=4 | O(0) | Replaced by inv_cdf |
| Pixels processed | P | P_eff ≤ P | Adaptive sampling |
| Samples per pixel | S | S_eff ≤ S | Per-pixel convergence |
| Hit record size | 20 bytes | 12 bytes | 40% less bandwidth |

### Remaining Bottleneck

The O(N) containment scan (`point_inside_ellipsoid` for every scene primitive) runs on every bounce. This is the only per-bounce step that scales with total scene size N rather than local overlap count A.

---

## Potential Future Optimizations

### Worth Implementing

**Next Event Estimation (NEE) — HIGH IMPACT**
At each scattering event, directly sample a direction toward the environment emitter and compute the transmittance along the shadow ray. Combine with indirect sampling via MIS. Currently the renderer relies on the phase function randomly hitting bright environment regions — NEE would reduce variance by 10-100× for environment-lit scenes. This is the single biggest quality improvement available. Requires: environment map importance sampling, phase function PDF evaluation, MIS weight computation, shadow ray transmittance (reuse existing segment-by-segment optical depth). ~200-300 lines.

**Anisotropic Phase Functions (Henyey-Greenstein) — MODERATE IMPACT, LOW EFFORT**
Replace the isotropic `1/(4π)` phase function with HG: `p(cos θ, g) = (1-g²) / (4π(1+g²-2g·cos θ)^{3/2})`. Parameter `g ∈ [-1,1]` controls forward/backward scattering (clouds: g≈0.85). Straightforward sampling exists. Per-primitive `g` could be stored alongside albedo. ~50 lines for sampling + evaluation.

**CPU-side Parallelism — DONE (std::execution::par)**
Applied to EXR export (row-level), PLY loading (primitive construction), renderer init (instance building, camera containment). Already implemented.

**Narrower Index Types — LOW EFFORT**
The `PrimsSet` (BitVector) and hit buffer use `uint` (32-bit) for primitive indices. For scenes with ≤65k primitives, `uint16_t` halves index storage and improves cache utilization. Similarly, `HIT_BUFFER_CAPACITY` and `ACTIVE_PRIMS_CAPACITY` could be `uint16_t`-indexed. Define a `prim_index_t` typedef to make this a single-point change.

**OptiX AI Denoiser — HIGH IMPACT, LOW EFFORT**
OptiX includes a built-in neural denoiser that produces clean images from noisy low-SPP renders. Could achieve visually converged results at 16-64 SPP instead of thousands. Requires: allocate denoiser state, feed it the noisy beauty pass (optionally albedo/normal AOVs for better quality), run as a post-process. ~50 lines of API calls. Orthogonal to all other optimizations.

**Low-Discrepancy Sequences (Sobol/Blue Noise) — MODERATE IMPACT**
Replace `curand` (independent random samples, O(1/√N) convergence) with Sobol or stratified sequences (O(1/N) for smooth integrands). For volumetric path tracing with many dimensions (pixel jitter, scatter direction, free-flight distance), Owen-scrambled Sobol is the standard choice. Requires: Sobol direction vectors as a device buffer, per-pixel dimension counter. ~100 lines. Expected 2-4× faster convergence (equivalent to 4-16× more samples).

**Precomputed `rcp(scale)` in Primitive — TRIVIAL**
`transform_pos_local` and `transform_dir_local` call `rcp(scale_)` every invocation. These are called 2× in `inv_cdf`, 2× in `optical_depth`, 1× in `pdf` — potentially 5+ reciprocal computations per primitive per ray. Storing `rcp_scale_` (float3) instead of `scale_` avoids this. GPU `__frcp_rn` is fast (1 cycle) but eliminating it entirely is free. Requires renaming `scale_` → `rcp_scale_` and adjusting the constructor + `scale()` getter.

**Early Optical Depth Termination — TRIVIAL**
In the escape segment integration, if accumulated τ exceeds a threshold (e.g., τ > 20, giving T < 2×10⁻⁹), skip remaining segments. The contribution is negligible. Add a single `if (acc > threshold) break;` to the escape loop.

**Launch Bounds / Occupancy Tuning — PROFILE FIRST**
The raygen kernel holds significant state (Welford accumulators, ScatteringEvent struct, hit buffer, RNG state). High register usage may limit occupancy. Use `__launch_bounds__(THREADS_PER_BLOCK, MIN_BLOCKS)` and NSight Compute to find the optimal balance. May require splitting the kernel or reducing live variables.

**Multi-Resolution Progressive Rendering — LOW EFFORT**
Render at reduced resolution first (e.g., 1/4), display a preview, then progressively refine to full resolution. Useful for interactive exploration. The batched rendering architecture already supports this — just change the launch dimensions between batches.

### Not Worth Implementing (without profiling evidence)

**Warp-Cooperative Model — NOT RECOMMENDED**
32 threads process one ray collaboratively instead of 32 rays independently. Helps the argmin loop (32 primitives in parallel instead of sequential) and escape integration, but hurts everywhere else: BVH traversal (31 threads idle during OptiX trace), low overlap counts (3-10 typical → 70-90% warp idle), path divergence after scattering. For typical DSYG scenes: argmin loop is ~20% of kernel time, 5× speedup on 20% = 4% net, minus BVH overhead → likely net negative. Only beneficial for dense media with dozens of overlapping volumes per ray.

**Warp-Cooperative Sorting — NOT RECOMMENDED**
Warp shuffle bitonic sort for escape-path events. Requires the warp-cooperative model (see above). The event count is small (2-20 typically), insertion sort handles it in nanoseconds, and the sort only runs on terminal escape bounces (once per path). Current adaptive sort is already near-optimal.

**SoA Memory Layout — NOT RECOMMENDED (without warp-cooperative model)**
An initial implementation exists at commit `eec7afd` with a clean `PrimitiveAccessor` pattern and `__ldg()` hints. However, in the current one-thread-per-ray model, SoA doesn't improve coalescing:
- Warp-coherent loops (all threads read same index): AoS works fine — same cache line
- Warp-divergent loops (different indices per thread): scattered regardless of layout
SoA only helps when consecutive threads read consecutive indices of the same field, which requires the warp-cooperative model. Additionally, maintaining duplicate math in both `Primitive` and `PrimitiveAccessor` creates a maintenance burden — every formula fix (like the argmin debugging) must be applied in two places.

Field access pattern for reference:
```
                          inv_cdf  optical_depth  pdf  evaluate_albedo
center_ (12B)               ✓          ✓           ✓
rot_quat_ (16B)             ✓          ✓           ✓
scale_ (12B)                ✓          ✓           ✓
density_norm_factor_ (4B)   -          ✓           ✓
inv_cdf_factor_ (4B)        ✓          -           -
albedo_ (12B)               -          -           -         ✓
optical_thickness_ (4B)     -          ✓           -         ✓
```

**Chunked PLY Loading (low priority)**
For very large PLY files (>50k primitives), overlap CPU primitive construction with GPU upload using double-buffered chunks. happly still loads all raw floats upfront (compact, ~40 bytes/prim), but construction + upload is pipelined:

```
CPU:  [construct chunk 0][construct chunk 1][construct chunk 2]...
GPU:           [upload 0 ][upload 1 ][upload 2 ]...
                          ↑ overlap ↑
```

Implementation sketch:
```cpp
constexpr size_t CHUNK_SIZE = 4096;
auto ply = happly::PLYData(filename);  // raw floats in memory
primitives_buffer.alloc(N);            // full device buffer upfront

// Double-buffer: construct into one while uploading the other
std::vector<Primitive> buf[2] = { std::vector<Primitive>(CHUNK_SIZE),
                                   std::vector<Primitive>(CHUNK_SIZE) };

for (size_t offset = 0; offset < N; offset += CHUNK_SIZE) {
    size_t count = std::min(CHUNK_SIZE, N - offset);
    auto& build_buf = buf[(offset / CHUNK_SIZE) % 2];

    stream_construct.sync();  // wait for prev upload of THIS buffer

    // Parallel construct (CPU)
    std::for_each(std::execution::par, iota(0, count), [&](size_t i) {
        build_buf[i] = Primitive(..., expf(scale[offset+i]), ...);
    });
    validate(build_buf, count);  // sequential, early exit

    // Async upload to device at correct offset (returns immediately)
    cudaMemcpyAsync(device_ptr + offset, build_buf.data(),
                    count * sizeof(Primitive), H2D, stream_upload);
}
stream_upload.sync();
build_bvh();
```

Current `std::execution::par` already parallelizes construction; this adds GPU upload overlap. Only meaningful at 50k+ primitives where construction takes tens of ms.

### Known Issues

**Stress test crashes (≥2048 Gaussians)**
`stress_{2048,4096,8192}_gaussians` crash with "illegal memory access" — likely hit buffer overflow or stack exhaustion from too many active primitives. The `HIT_BUFFER_CAPACITY` and stack size may need scaling for these counts, or a graceful fallback when capacity is exceeded.
