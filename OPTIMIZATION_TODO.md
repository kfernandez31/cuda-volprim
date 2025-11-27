# Future Optimization Opportunities

This document outlines potential performance optimizations for the volumetric path tracer after the basic anyhit collection approach is working.

**Note:** This document now incorporates findings from analyzing the Mitsuba `volumetric_primitives` reference implementation.

## 1. Sorting Optimization ✅ IMPLEMENTED

**Status:** Adaptive hybrid sort implemented in `device/core/sorting.cuh`

### Current Implementation
```cuda
if (n <= 64) insertion_sort();      // O(n²), excellent cache behavior
else bitonic_sort();                 // O(n log² n) sorting network
```

**Algorithms used:**
- **Insertion sort (n ≤ 64):** Simple, low overhead, good for small/nearly-sorted arrays
- **Bitonic sort (n > 64):** Parallel sorting network, O(n log² n), works on single thread

**Expected gain:** 10-50x over original bubble sort

### Why Not Warp Shuffle Sort?

**Issue:** Warp shuffle sorting requires all 32 threads in a warp to cooperate on sorting **one shared vector**. In our execution model:
- Each thread has its **own independent vector** to sort
- Threads don't share vectors (one thread = one ray = one hit buffer)
- Warp shuffles would mix data between different threads' vectors ❌

**When warp shuffle would work:**
- Execution model: One warp = One ray (all 32 threads process same ray)
- Requires major raygen refactor
- Current model: One thread = One ray (independent processing) ✅

### Future: Warp-Cooperative Sorting (Phase 3)

If profiling shows sorting is still a bottleneck, could implement:
1. Coordinate all 32 threads in warp to sort one thread's vector at a time
2. Process 32 vectors sequentially (each with warp cooperation)
3. **Problem:** 32× sequential overhead likely negates warp shuffle benefit

**Verdict:** Current insertion + bitonic approach is optimal for our execution model

### Option B: CUB DeviceRadixSort
Use NVIDIA's optimized library for sorting.
**Pros:** Battle-tested, near-optimal
**Cons:** Requires dynamic memory allocation, may not work in device code
**Expected gain:** 50-100x

### Option C: Early Termination with Quick Select
```cuda
quick_select(buffer, k=128);  // Find 128 smallest O(n)
sort(buffer, 0, 128);         // Sort first batch
process_batch();
if (scattering) return;       // Exit early!
// Otherwise continue with next batch
```
**Expected gain:** 3-10x if scattering happens early

---

## 2. Parallel Integration (Warp-Level)

**Current:** Sequential loop over active primitives

### Challenge: Requires Execution Model Change

Current model: **One thread = One ray**
- Thread 0 processes ray A independently
- Thread 1 processes ray B independently
- No cooperation between threads

Warp-parallel requires: **One warp = One ray**
- All 32 threads cooperate on same ray
- Requires major raygen refactoring

### Option A: Cooperative Warp Processing
```cuda
__global__ void raygen_cooperative() {
    const int warp_id = threadIdx.x / 32;
    const int lane_id = threadIdx.x & 31;

    // Only lane 0 picks ray
    Ray ray;
    if (lane_id == 0) {
        ray = get_ray_for_warp(warp_id);
    }
    // Broadcast to all lanes
    ray = warp_broadcast(ray, 0);

    // Now all 32 threads process same ray cooperatively
    // Warp-parallel integration makes sense here
}
```
**Effort:** Very high (complete raygen restructure)
**Expected gain:** 10-30x on integration when many primitives active
**Risk:** High - changes fundamental execution model

### Option B: SIMD/Vectorization (Simpler Alternative)
Use `float4` and vector instructions instead of warp cooperation:
```cuda
// Process 4 primitives per instruction
float4 density = compute_density_vec4(prims, ray, t0, t1);
acc += density.x + density.y + density.z + density.w;
```
**Effort:** Medium (data layout changes)
**Expected gain:** 1.5-3x
**Risk:** Medium - requires AoS → SoA conversion

---

## 2.5 Warp-Level Primitives (Middle Ground)

**Current:** Sequential loop over active primitives, no warp cooperation
**Proposed:** Use warp shuffle intrinsics for parallel reductions without execution model change

**Impact:** 1.5-2x for optical depth computation (if bottleneck)

### Problem

Current optical depth accumulation (`sampling.cuh:58-71`):
```cuda
__device__ float optical_depth_accumulated(
    const geometry::Ray& ray,
    const PrimsSet& prims,
    float t0, float t1
) {
    auto tau = 0.0f;

    // Each thread loops sequentially
    for (auto idx : prims) {
        const auto& prim = launch_params.primitives_[idx];
        tau += prim.optical_depth(ray, t0, t1);  // Expensive: erf functions
    }

    return tau;
}
```

**Issue:**
- If `prims.size() = 64`, thread processes 64 primitives **sequentially**
- Other threads in warp doing the same for their rays
- **No cooperation** - each thread works independently

---

### Solution: Warp-Level Reduction

**Key insight:** Maintain current execution model (one thread = one ray), but use warp cooperation **within** per-thread loops.

**Strategy:**
1. Each thread distributes its primitives across the warp
2. Warp lanes process different primitives in parallel
3. Reduce results back to owning thread

```cuda
__device__ float optical_depth_accumulated_warp(
    const geometry::Ray& ray,
    const PrimsSet& prims,
    float t0, float t1
) {
    const int lane_id = threadIdx.x & 31;
    const int num_prims = prims.size();

    float tau_local = 0.0f;

    // Distribute primitives across warp lanes (stride=32)
    for (int i = lane_id; i < num_prims; i += 32) {
        const auto idx = prims[i];
        const auto& prim = launch_params.primitives_[idx];
        tau_local += prim.optical_depth(ray, t0, t1);
    }

    // Warp-level reduction to sum all lanes' contributions
    tau_local = warp_reduce_sum(tau_local);

    return tau_local;
}

// Warp reduction utility
__device__ float warp_reduce_sum(float val) {
    #pragma unroll
    for (int offset = 16; offset > 0; offset >>= 1) {
        val += __shfl_down_sync(0xFFFFFFFF, val, offset);
    }
    return val;  // Lane 0 has sum, other lanes have partial sums (ignored)
}
```

---

### How It Works

**Example: 64 primitives, 32-thread warp**

**Sequential (current):**
```
Thread 0: Process prims 0-63 sequentially (64 iterations)
Thread 1: Process prims 0-63 sequentially (64 iterations)
...
Thread 31: Process prims 0-63 sequentially (64 iterations)
```
Each thread does 64 optical_depth calls.

**Warp-parallel:**
```
Lane 0:  Process prims 0, 32      (2 iterations)
Lane 1:  Process prims 1, 33      (2 iterations)
...
Lane 31: Process prims 31, 63     (2 iterations)

Then: Warp-reduce to sum all partial results
```
Each lane does 2 optical_depth calls, then cooperate for reduction.

**Net effect:** 2× parallelism (32 lanes working simultaneously on same ray's primitives)

---

### Advantages

**vs. Full Warp-Cooperative (Section 2):**
- ✅ **No execution model change** - still one thread = one ray
- ✅ **No raygen refactoring** - just change optical_depth function
- ✅ **Simple to implement** - ~30 lines of code
- ✅ **Low risk** - warp shuffles are well-tested

**vs. Current Sequential:**
- ✅ **2× fewer iterations per thread** (32-way parallelism)
- ✅ **Better instruction throughput** - more warps can be active
- ✅ **Same memory footprint** - no additional storage needed

**Limitations:**
- ⚠️ **Only helps when many primitives active** (>32 for benefit)
- ⚠️ **All lanes must process same ray** - wastes lanes if primitives < 32
- ⚠️ **Load imbalance** if primitive count not multiple of 32

---

### Implementation Strategy

**Step 1: Add warp reduction utility** (`device/core/warp_utils.cuh`):
```cuda
__device__ float warp_reduce_sum(float val) {
    #pragma unroll
    for (int offset = 16; offset > 0; offset >>= 1) {
        val += __shfl_down_sync(0xFFFFFFFF, val, offset);
    }
    return val;
}

__device__ float3 warp_reduce_sum(float3 val) {
    return make_float3(
        warp_reduce_sum(val.x),
        warp_reduce_sum(val.y),
        warp_reduce_sum(val.z)
    );
}
```

**Step 2: Convert optical_depth_accumulated to warp version:**

Replace `sampling.cuh:58-71` with warp-parallel version shown above.

**Step 3: Convert integrate_primitives similarly** (`sampling.cuh:73-101`):
```cuda
__device__ float3 integrate_primitives_warp(...) {
    const int lane_id = threadIdx.x & 31;
    float3 result_local = make_float3(0.0f);

    for (int i = lane_id; i < prims.size(); i += 32) {
        const auto idx = prims[i];
        const auto& prim = launch_params.primitives_[idx];
        result_local += prim.density_integral(ray, t0, t1);
    }

    return warp_reduce_sum(result_local);
}
```

---

### Effort: Medium

- Implement warp reduction utilities (~30 lines)
- Convert 2-3 functions to warp-parallel versions (~50 lines)
- Test for correctness (warp reductions must be correct!)
- Handle edge cases (primitive count < 32)

**Total:** ~100 lines of new code

---

### Expected Gain: 1.5-2x (if bottleneck)

**Conditions for benefit:**
- Optical depth computation is >30% of total time (profile first!)
- Scenes with many overlapping primitives (>32 active at once)
- High bounce counts (MAX_BOUNCES > 64)

**Diminishing returns when:**
- Few primitives active (<16) - wasted warp lanes
- Optical depth not the bottleneck (BVH traversal or sorting dominates)

---

### Recommendation: Phase 3 (Profile-Driven)

**Don't implement this blindly:**
1. **Profile first** with `nsys` or NSight Compute
2. **Identify if** `optical_depth_accumulated()` is >30% of total time
3. **If yes:** Implement warp-level version
4. **If no:** Focus on actual bottleneck (likely sorting or BVH traversal)

**This is a middle ground** between:
- **Current:** Simple, sequential, no cooperation
- **Full warp-cooperative (Section 2):** Complex, major refactor, theoretical 10-30x

---

## 3. Memory Optimization

### Current Issue: Stack Pressure

**Current usage per thread:**
```
HitBuffer: 2000 × 12 bytes = 24 KB
PrimsSet:  2000 × 4 bytes  = 8 KB
Locals:    ~1 KB
Total:     ~33 KB per thread
```

**CUDA default:** 1 KB per thread
**Problem:** Severe occupancy reduction

### Option A: Reduce Buffer Size (QUICK WIN)
```cpp
constexpr size_t MAX_CAPACITY = 256;  // 3 KB vs 24 KB
```
- Most rays hit < 50 primitives anyway
- Handle overflow gracefully (truncate/warn)
- Significantly improves occupancy

**Effort:** Minimal (change one constant)
**Expected gain:** 5-10x better occupancy → more warps active

### Option B: Global Memory Pool
```cuda
HitBuffer* d_hit_buffers;
cudaMalloc(&d_hit_buffers, max_concurrent_rays * sizeof(HitBuffer));
// Pass pointer per ray
```
**Pros:** No stack pressure, large buffers
**Cons:** Allocation overhead, global memory latency
**Effort:** Medium

### Option C: Shared Memory
```cuda
__shared__ HitBuffer shared_buffers[BLOCK_SIZE];
// Each thread uses index within block
```
**Pros:** 48 KB shared mem per block, fast access
**Cons:** Limited capacity, synchronization needed
**Effort:** Medium-High

---

## 4. Early Termination Strategies - will not do

### Current: Always sort entire buffer before processing

### Option A: Streaming/Batched Processing
```cuda
while (!scattered && buffer_has_more()) {
    auto batch = get_next_sorted_batch(k=100);
    for (hit : batch) {
        process(hit);
        if (scattered) return;
    }
}
```
**Expected gain:** 3-10x if scattering is typically early

### Option B: Probabilistic Skip
```cuda
// Sample 10% of hits, estimate tau_total
if (estimated_tau < tau_target * 0.5) {
    // Unlikely to scatter, sort all
} else {
    // Use batched approach
}
```
**Expected gain:** Variable, depends on scene

---

## 5.5. Direct Distance Sampling (Jorge's Suggestion)

**Current:** Sample target optical depth `τ_target`, then use bisection to invert and find distance `t`

**Jorge's Idea:** Directly sample a distance `t` along the segment proportional to the density distribution

### Problem with Current Approach
```cuda
// sampling.cuh:105-133
float tau_target = sample_target_optical_depth(chi);  // Step 1: Sample τ
// Step 2: Expensive bisection search (24 iterations!)
float t_scatter = sample_distance_bisection(ray, active_prims, tau_needed, t_prev_hit, t_hit);
```

**Why it's expensive:**
- Bisection requires 24 iterations (MAX_ITER = 24)
- Each iteration calls `optical_depth_accumulated()` which loops over all active primitives
- Each primitive call evaluates expensive `erf()` functions for Gaussian integration

### Jorge's Optimization
Instead of "sample τ → invert to find t", directly sample `t` from the density distribution:

```cuda
// Direct sampling (if analytically tractable)
float t_scatter = sample_distance_direct(ray, active_prims, t_prev_hit, t_hit, rng);
```

### Possible Approaches

**Option A: Rejection Sampling**
```cuda
float sample_distance_rejection(Ray ray, PrimsSet prims, float t0, float t1, curandState& rng) {
    // Find max density along segment
    float density_max = compute_max_density(ray, prims, t0, t1);

    while (true) {
        float t_candidate = t0 + random::sample_uniform(rng) * (t1 - t0);
        float density = evaluate_density(ray, prims, t_candidate);

        if (random::sample_uniform(rng) * density_max < density) {
            return t_candidate;  // Accepted!
        }
    }
}
```
**Pros:** Simple, works for arbitrary densities
**Cons:** Requires max density search, rejection rate depends on distribution shape

**Option B: Importance Sampling (if CDF is tractable)**
For simple cases (single Gaussian, uniform density), we might be able to analytically invert the CDF:
```cuda
// Example: uniform density
float t = t0 + chi * (t1 - t0);  // Direct, no bisection needed!

// Example: single exponential
float t = t0 - ln(1 - chi) / sigma;

// For Gaussian: CDF involves erf, but might be invertable
```
**Pros:** One-shot sampling, no iterations
**Cons:** Only works for simple cases; overlapping Gaussians make CDF intractable

**Option C: Discretize + Alias Method**
Pre-discretize the segment into bins, build alias table:
```cuda
// Preprocessing (per segment)
float bins[N];
for (int i = 0; i < N; i++) {
    bins[i] = integrate_density(t0 + i*dt, t0 + (i+1)*dt);
}
AliasTable table(bins);

// Sampling (O(1))
int bin_idx = table.sample(rng);
float t = t0 + bin_idx * dt + random::sample_uniform(rng) * dt;
```
**Pros:** O(1) sampling after preprocessing
**Cons:** Preprocessing overhead, discretization error

### Challenges
- **Overlapping Gaussians:** Sum of Gaussians has no closed-form CDF inversion
- **Varying active sets:** Different segments have different primitives active
- **Complexity vs. Gain:** Bisection is already fast enough for most cases

### Effort: Medium-High
Requires careful mathematical analysis and validation against current bisection approach.

### Expected Gain: 2-5x
Eliminates 24 iterations of optical depth evaluation per scattering event.

### Risk: Medium
- May only be tractable for simple cases (single Gaussian, uniform density)
- Complex cases (overlapping Gaussians) might still need bisection fallback
- Correctness validation is critical (wrong sampling = biased renders)

### Recommendation: Phase 3 (Profile-Driven)
Only pursue if profiling shows `sample_distance_bisection()` is a bottleneck (>20% of total time). For most scenes with few scattering events, the benefit may not justify the implementation complexity.

**References:**
- `sampling.cuh:103` - Original Jorge comment
- `sampling.cuh:105-133` - Current bisection implementation

---

## 6. OptiX Features

### optixReorder (Ray Coherence) - ❌ NOT COMPATIBLE

**Status:** Cannot use with current architecture

**Why it doesn't work:**
- `optixReorder()` requires the **hit object API** (`optixTraverse()` + `optixInvoke()`)
- Must be called **after** `optixTraverse()` (when hit object is available) but **before** `optixInvoke()` (shader execution)
- Our current implementation uses `optixTrace()` which combines traversal and shading in one call
- Calling `optixReorder()` before `optixTrace()` has no effect (no hit object exists yet)

**From OptiX docs:**
> "Ideally, reordering should therefore occur after a ray has been cast (that is, when we know the hit location in the scene), but before further shading takes place."

**Required refactor:**
Replace `optixTrace()` with:
```cuda
optixTraverse(...);          // Get hit object
optixReorder(coherenceHint, numBits);  // Reorder based on hit
optixInvoke(...);            // Execute shading
```

**Additional notes:**
- OptiX docs recommend **NOT** using reordering for trivial shaders or coherent rays (like primary rays)
- Our raygen shader does all processing inline, so benefit would be minimal anyway
- Only useful with complex closesthit/miss shaders accessing diverse data

**Effort:** Very High (requires switching entire execution model)
**Expected gain:** Unclear (may not benefit our architecture)
**Priority:** Not recommended for current implementation


### Recommendation: Phase 2 (After Core Works)

- Implement after coincident surfaces are fixed
- Clear win with minimal code change
- No algorithm complexity (just hardware feature utilization)

**References:**
- CUDA Texture Memory Programming Guide
- OptiX examples using texture objects for HDR environment maps

---

## 7. Warp-Cooperative Hit Collection (User Idea)

**Current:** Each thread independently collects all hits along its ray sequentially

**Your Idea:** Partition ray traversal across warp lanes, collect in parallel, then merge

### Concept
```
Ray: [-------- 0 to infinity --------]
     |   |   |   |   |   |   |   |   |
   L0  L1  L2  L3  ...             L31  (32 lanes)

Each lane traces its own segment in parallel!
```

### Option A: Partition Ray by t-Range
Each lane traces a different segment of the ray:

```cuda
__global__ void raygen_warp_cooperative() {
    const int lane_id = threadIdx.x & 31;

    // Broadcast same ray to all lanes in warp
    Ray ray;
    if (lane_id == 0) {
        ray = get_ray_for_warp();
    }
    ray = warp_broadcast(ray, 0);  // All lanes get same ray

    // Each lane collects hits in its segment
    const float t_segment_size = MAX_DISTANCE / 32;
    const float t_min = lane_id * t_segment_size;
    const float t_max = (lane_id + 1) * t_segment_size;

    HitBuffer local_hits;  // 64 hits per lane (vs 2000 for single thread)
    trace_ch_collect(ray, t_min, t_max, &local_hits);

    // Merge all 32 buffers across warp
    HitBuffer merged = warp_merge_buffers(local_hits);

    // Sort once (all lanes participate)
    warp_sort(merged);

    // Process hits (cooperative or lane 0 only)
    for (hit : merged) {
        // ... scattering logic ...
    }
}
```

**Advantages:**
- **32x parallel BVH traversal** - Each lane traces independently
- **Smaller per-lane buffers** - 32 lanes × 64 hits = 2048 total capacity
- **Better memory/occupancy** - Less stack pressure per thread
- **Natural load distribution** - Lanes finish segments at different rates

**Challenges:**
- **Requires cooperative warp model** - Major raygen refactor (same as warp-parallel integration)
- **Merging is expensive** - Need to concatenate 32 unsorted buffers, then sort ~2000 hits
- **Uneven hit distribution** - What if 90% of hits are in lanes 0-5? Those lanes overflow, others idle
- **OptiX semantics** - Need to verify OptiX supports same ray with different t_min/t_max per thread
- **Dynamic work redistribution** - May need load balancing if distribution is very uneven

### Option B: Partition by Primitive ID
Instead of ray segments, each lane checks different primitives:

```cuda
__global__ void raygen_warp_cooperative() {
    const int lane_id = threadIdx.x & 31;
    Ray ray = warp_broadcast(get_ray(), 0);

    const int num_instances = get_num_instances();

    HitBuffer my_hits;
    for (int i = lane_id; i < num_instances; i += 32) {
        // Trace ray against ONLY primitive i
        // Requires custom trace that filters by instance ID
        trace_single_instance(ray, instance_id=i, &my_hits);
    }

    // Merge buffers from all lanes
    HitBuffer merged = warp_merge_buffers(my_hits);
}
```

**Advantages:**
- More even distribution (primitives spread uniformly)
- Avoids t-range partitioning problems

**Disadvantages:**
- Requires per-primitive traces (OptiX may not support this efficiently)
- Could be slower than single full trace

### Effort: Very High
- Complete raygen restructure (cooperative warp model)
- Complex buffer merging logic (concatenate + sort)
- Warp-level synchronization and communication
- Significant testing/debugging
- May need custom OptiX trace variants

### Expected Gain: 5-20x (if BVH traversal is bottleneck)
If most time is spent in BVH traversal, parallelizing collection across 32 lanes could give massive speedup. However, merging overhead could reduce gains.

### Open Questions:
1. Can OptiX trace same ray with different t_min/t_max per thread efficiently?
2. What's the overhead of merging 32 buffers vs single collection?
3. How uneven is hit distribution in practice?

### Recommendation: Phase 3 (Profile-Driven)
- First implement basic anyhit collection
- Profile to identify if BVH traversal is the bottleneck
- If BVH dominates (>50% of time), this becomes high priority
- Consider prototyping Option A to measure actual speedup vs overhead

---

### Warp Divergence and Idle Threads (Context Note)

**Current behavior with `MAX_BOUNCES = 1`:**
- Minimal warp divergence (most rays process single bounce)
- Early termination via RR or ray escape doesn't cause significant idle time

**With `MAX_BOUNCES = 128` (recommended, see Section 11):**
- **Significant warp divergence becomes an issue**

**Divergence sources:**
1. **Ray escape** (`raygen.cuh:73`) - Some rays hit environment, terminate early
2. **Russian Roulette** (`raygen.cuh:96`) - Probabilistic termination after bounce 64
3. **NaN detection** (`raygen.cuh:104`) - Rare numerical errors terminate path

**What happens:**
```
Warp with 32 threads at bounce 70:
Thread 0: Terminates (RR)          → IDLE
Thread 1: Still active             → WORKING
Thread 2: Terminates (RR)          → IDLE
Thread 3: Still active             → WORKING
...
Thread 31: Terminates (escaped)    → IDLE

Result: Warp executes with 40% idle threads (wasted cycles)
```

**Impact:**
- **Low bounce counts (1-32):** Minimal divergence, most threads active
- **Medium bounce counts (32-64):** Some divergence before RR kicks in
- **High bounce counts (64-128):** Heavy divergence, 50-70% of threads idle after RR

**Stream compaction would help:**
- Repack active rays into contiguous work queue
- Launch new kernel with only active rays
- Avoid idle threads waiting for warp completion

**But requires:**
- Major execution model change (work queue, multiple kernel launches)
- Overhead of compaction (prefix sum, array writes)
- Only beneficial if warp divergence >50% AND sorting/integration aren't bottlenecks

**Recommendation:**
- **Don't implement** unless profiling shows >50% warp idle time
- **Alternative:** Keep bounce count moderate (32-64) to minimize divergence
- **Phase 4** consideration after all other optimizations exhausted

```
## 9. Next Event Estimation (NEE) / Direct Lighting

**Current:** Relying on random phase function sampling to hit environment emitter (high variance)
**Mitsuba reference:** Implements NEE at every scattering event (`volprim_prb.py:195-235`)

**Impact:** **10-100x faster convergence** for scenes with emitters

### Problem

Without NEE, your path tracer:
1. Scatters at medium interaction
2. Samples random direction from phase function (isotropic = uniform sphere)
3. Hopes that direction eventually hits the environment emitter
4. Very low probability → very high variance → slow convergence

With NEE:
1. Scatters at medium interaction
2. **Directly samples a direction toward the emitter**
3. Computes transmittance along shadow ray
4. Uses MIS to combine with indirect sampling
5. Much higher probability of connecting to light → low variance → fast convergence

### Implementation

**Step 1: Direct emitter sampling at scattering point**
```cuda
// After scattering event in raygen.cuh
if (result) {  // Scattering occurred
    auto albedo = evaluate_albedo(event.position_, event.active_prims_);

    // NEE: Sample direction toward environment emitter
    float2 sample_2d = random::sample_uniform_2d(rng);
    float3 light_dir = launch_params.env_map_.sample_direction(sample_2d);
    float light_pdf = launch_params.env_map_.pdf(light_dir);

    // Spawn shadow ray
    auto shadow_ray = geometry::Ray::spawn_unchecked(event.position_, light_dir);

    // Compute transmittance to infinity (no surface intersection check needed for env)
    auto tau_shadow = compute_optical_depth_along_ray(shadow_ray, event.active_prims_);
    auto transmittance = expf(-tau_shadow);

    // Evaluate phase function for this direction
    // For isotropic: phase_pdf = 1 / (4π), phase_value = 1 / (4π)
    float phase_pdf = consts::PHASE_VALUE;

    // Multiple Importance Sampling (balance heuristic)
    float weight_nee = mis_weight(light_pdf, phase_pdf);

    // Add NEE contribution
    auto emitter_val = launch_params.env_map_.sample(light_dir);
    radiance += throughput * albedo * weight_nee * transmittance * emitter_val / light_pdf;

    // Then continue with indirect sampling as before
    // ...
}
```

**Step 2: Update environment map to support direction sampling**

Need to add to environment map:
```cuda
// Sample direction toward emitter (importance sampling)
__device__ float3 sample_direction(float2 sample) const;

// PDF of sampling that direction
__device__ float pdf(float3 direction) const;
```

**Step 3: Implement MIS weight**
```cuda
// Balance heuristic (optimal for 2 sampling strategies)
__device__ float mis_weight(float pdf_a, float pdf_b) {
    return (pdf_a * pdf_a) / (pdf_a * pdf_a + pdf_b * pdf_b);
}
```

**Step 4: Disable indirect emitter hits if NEE is on**

To avoid double-counting, when NEE is enabled:
```cuda
// When ray escapes without scattering
if (!result) {
    auto tau = compute_optical_depth_along_ray(ray, event.active_prims_);

    // If NEE is enabled and depth > 0, don't count indirect emitter hits
    // (they're already accounted for by NEE)
    if (bounce == 0) {  // Only count direct camera-to-emitter rays
        radiance += (throughput * expf(-tau)) * miss.color();
    }
    // Otherwise, transmittance already applied via NEE
    break;
}
```

### Required Components

1. **Environment map direction sampling** - Importance sample directions toward bright regions
2. **Environment map PDF evaluation** - Compute probability of sampling a given direction
3. **Shadow ray transmittance** - Reuse `compute_optical_depth_along_ray()`
4. **MIS weight computation** - Balance heuristic between NEE and indirect
5. **Phase function PDF** - For isotropic: constant `1/(4π)`

### Effort: Medium-High

- Need to implement importance sampling for environment map (non-trivial)
- Need to handle MIS correctly
- Need to modify raygen loop structure
- Roughly 200-300 lines of new code

### Expected Gain: 10-100x

- Scenes with strong directional lighting: 100x faster convergence
- Scenes with uniform environment: 10x faster convergence
- Critical for practical rendering

### Recommendation: Phase 2 (High Priority)

After core features work (coincident surfaces, geometry validation), NEE should be the next major feature.

**Alternative approach:** For thesis validation, you could compare:
- Your renderer WITHOUT NEE
- Mitsuba WITH NEE disabled (`use_nee=False`)
- This isolates the algorithm comparison from the NEE optimization

---

## 10. Anisotropic Phase Functions

**Current:** Isotropic phase function (uniform scattering over sphere)
**Mitsuba reference:** Supports spatially-varying phase functions per primitive

**Impact:** More physically accurate scattering, directional effects like forward/backward scattering

### Problem

Isotropic phase function (`sampling.cuh:32-45`):
```cuda
__forceinline__ __device__ float3 sample_phase(curandState& rng) {
    // Uniform sampling over unit sphere
    // Always returns phase_pdf = 1/(4π)
}
```

**Limitations:**
- No directional preference (real particles scatter anisotropically)
- Cannot model Mie scattering (water droplets, fog)
- Cannot model Rayleigh scattering (atmosphere)
- Cannot model forward-scattering (smoke, dust)

### Anisotropic Phase Functions

**Henyey-Greenstein (most common):**
```cuda
__device__ float phase_hg(float cos_theta, float g) {
    // g ∈ [-1, 1]: anisotropy parameter
    // g = 0: isotropic
    // g > 0: forward scattering
    // g < 0: backward scattering
    float denom = 1.0f + g*g - 2.0f*g*cos_theta;
    return (1.0f / (4.0f * PI)) * (1.0f - g*g) / (denom * sqrtf(denom));
}

__device__ float3 sample_phase_hg(curandState& rng, float g) {
    // Importance sample HG phase function
    float cos_theta;
    if (abs(g) < 1e-3f) {
        cos_theta = 1.0f - 2.0f * random::sample_uniform(rng);  // Isotropic fallback
    } else {
        float sqr_term = (1.0f - g*g) / (1.0f - g + 2.0f*g*random::sample_uniform(rng));
        cos_theta = (1.0f + g*g - sqr_term*sqr_term) / (2.0f * g);
    }

    // Convert to 3D direction (similar to isotropic)
    float sin_theta = sqrtf(fmaxf(0.0f, 1.0f - cos_theta*cos_theta));
    float phi = 2.0f * PI * random::sample_uniform(rng);
    return make_float3(sin_theta * cosf(phi), sin_theta * sinf(phi), cos_theta);
}
```

**Other phase functions:**
- **Mie scattering:** Complex wavelength-dependent (atmosphere, water droplets)
- **Rayleigh scattering:** `p(θ) ∝ (1 + cos²θ)` (blue sky)
- **Schlick approximation:** Cheaper alternative to HG
- **Double HG:** Mixture of two HG lobes (more flexibility)

### Implementation Strategy

**Step 1: Add phase function interface**
```cuda
struct PhaseFunction {
    enum Type { Isotropic, HenyeyGreenstein, Rayleigh };
    Type type;
    float g;  // For HG: anisotropy parameter

    __device__ float3 sample(curandState& rng, float3 wi) const;
    __device__ float eval(float3 wi, float3 wo) const;
    __device__ float pdf(float3 wi, float3 wo) const;
};
```

**Step 2: Add to primitive parameters**
```cuda
class Primitive {
    PhaseFunction phase_;  // Per-primitive phase function
    // ...
};
```

**Step 3: Update scattering sampling**
```cuda
// In raygen.cuh, after scattering event:
auto phase_fn = get_phase_function(event.position_, event.active_prims_);
event.direction_ = phase_fn.sample(rng, -ray.direction_);  // Note: depends on incident direction
```

**Step 4: Update NEE (when implemented)**
```cuda
// Need to evaluate phase function for specific direction
float phase_pdf = phase_fn.pdf(-ray.direction_, light_dir);
float phase_val = phase_fn.eval(-ray.direction_, light_dir);
```

### Use Cases

**Forward scattering (g > 0):**
- Fog, smoke, clouds
- Subsurface scattering in translucent materials
- Typical values: g ∈ [0.3, 0.9]

**Backward scattering (g < 0):**
- Some types of dust particles
- Less common in practice

**Rayleigh scattering:**
- Atmosphere rendering (sky color)
- Small particles (much smaller than wavelength)

### Effort: Medium

- Implement HG phase function (100 lines)
- Add phase function parameters to primitives (50 lines)
- Update scattering sampling and NEE (100 lines)
- Roughly 250-300 lines total

### Expected Gain: Visual Quality

- **Performance:** Negligible impact (sampling is similar cost to isotropic)
- **Quality:** Dramatically more realistic scattering
- **Physically accurate:** Matches real-world particle behavior

### Recommendation: Phase 3 (After NEE)

Anisotropic phase functions significantly improve visual realism, but:
1. Implement NEE first (bigger convergence win)
2. Validate basic isotropic rendering works correctly
3. Then add anisotropic phase as visual quality enhancement

**For thesis:**
- Compare isotropic vs HG with different g values
- Show forward scattering effects (fog/smoke appearance)
- Benchmark performance impact (should be minimal)

**References:**
- Henyey & Greenstein (1941): Original paper
- PBR Book Chapter 11.3: Phase Functions
- Mitsuba documentation: Phase function plugins

---

## 10. Remove GLM Dependency (Code Simplification)

**Current:** Host code uses GLM library for vector math and transforms

**Motivation:** Reduce external dependencies, simplify build, use native CUDA types throughout

### What GLM is used for:
- `glm::vec3`, `glm::vec4` for host-side vector math
- `glm::mat4` for transforms
- `glm::quat` for rotations
- Conversion functions (`toFloat3`, `toFloat4`) to bridge GLM ↔ CUDA types

### Replacement Strategy:

**Option A: Use CUDA vector types directly**
```cpp
// Current (io.cpp:198-201)
const auto center = glm::vec3(p_x[i], p_y[i], p_z[i]);
const auto scale = glm::vec3(scale_0[i], scale_1[i], scale_2[i]);

// Replacement: use float3 directly
const auto center = make_float3(p_x[i], p_y[i], p_z[i]);
const auto scale = make_float3(scale_0[i], scale_1[i], scale_2[i]);
```

**Option B: Implement minimal math utilities**
For transforms and quaternions, implement only what's needed:
```cpp
// Replace glm::mat4 with simple 4x4 matrix struct
struct Mat4 {
    float m[16];

    static Mat4 from_trs(float3 translation, Quat rotation, float3 scale);
    Mat4 transpose() const;
};

// Replace glm::quat
struct Quat {
    float x, y, z, w;

    Mat4 to_matrix() const;
};
```

**Files to modify:**
- `src/thesis/host/utils/io.cpp` - GLM vector construction
- `src/thesis/host/app/renderer.cpp` - Transform matrices
- `include/thesis/host/utils/data.h` - Remove `toFloat3`/`toFloat4` conversion helpers
- `include/thesis/host/params/primitive.h` - Transform utilities

### Effort: Medium
- Need to implement matrix/quaternion utilities (~200 lines)
- Update all GLM usage sites (~10 files)
- Test transform correctness

### Expected Gain:
- **Compile time:** Slightly faster (one less dependency to parse)
- **Binary size:** Negligible (GLM is header-only)
- **Code clarity:** Moderate improvement (fewer type conversions)
- **Maintenance:** Easier (fewer dependencies, simpler build)

### Risk: Low
- Math utilities are straightforward to implement
- Easy to validate correctness (compare transforms before/after)
- Can be done incrementally (GLM and native types can coexist during transition)

### Recommendation: Phase 4 (Post-Optimization)
This is a code quality improvement, not a performance optimization. Consider after:
1. Core algorithm is validated and working
2. Performance optimizations are complete
3. You want to clean up the codebase for thesis submission

---

## Priority Ranking

### Phase 1: Low-Hanging Fruit (Do First)
1. **optixReorder** (1 line of code, 1.5-3x win for multi-sample rendering)
2. **Reduce MAX_CAPACITY to 256-512** (instant occupancy win, test for overflow)
3. **Add adaptive sort** (medium effort, 10-100x gain on sorting)
4. **Loop unrolling** (compiler directive, free 1.2-1.5x)

### Phase 2: Moderate Effort (Profile-Driven)
5. **Early termination with batching** (if scattering typically happens early)
6. **SIMD vectorization** (if profiling shows integration is bottleneck)

### Phase 3: Major Refactors (Only After Profiling)
7. **Warp-cooperative hit collection** (if BVH traversal dominates >50%)
8. **Global memory pool** (if occupancy still critical after reducing buffer)
9. **Cooperative warp integration** (if integration dominates >30%)

---

## How to Proceed

1. **Get basic anyhit collection working first!**
2. **Profile with `nsys`/`nvprof`** - Identify actual bottlenecks
   - How much time in BVH traversal vs sorting vs integration?
   - What's the hit count distribution? (Informs buffer size)
   - Where does scattering typically occur? (Informs early termination)
3. **Start with Phase 1** optimizations (proven wins, low risk)
4. **Measure each change** to validate improvements
5. **Only proceed to Phase 2/3** if profiling shows specific bottlenecks

**Tools:**
- `nsys profile --stats=true ./your_app` - NVIDIA Nsight Systems
- `ncu` - NVIDIA Nsight Compute for kernel analysis
- CUDA occupancy calculator
- OptiX profiler hooks (`optixQueryStateProp`)
