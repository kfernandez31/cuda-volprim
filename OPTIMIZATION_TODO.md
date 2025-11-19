# Future Optimization Opportunities

This document outlines potential performance optimizations for the volumetric path tracer after the basic anyhit collection approach is working.

---

## 1. Sorting Optimization

**Current:** O(n²) bubble sort for up to 2000 hits per ray

### Option A: Adaptive Hybrid Sort (RECOMMENDED)
```cuda
if (n <= 32) warp_sort();           // Shuffle-based, ~100x faster
else if (n <= 128) insertion_sort(); // O(n²) but fast for small n
else if (is_pow2(n)) bitonic_sort(); // O(n log² n)
else radix_sort_device();            // O(n), needs temp storage
```
**Effort:** Medium
**Expected gain:** 10-100x for typical hit counts

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

## 4. Early Termination Strategies

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

## 5. Instruction-Level Parallelism (ILP)

### Loop Unrolling
```cuda
// Instead of:
for (int i = 0; i < n; i++) {
    acc += prim[i].integrate(...);
}

// Unroll 4x:
for (int i = 0; i < n; i += 4) {
    acc += prim[i+0].integrate(...);
    acc += prim[i+1].integrate(...);
    acc += prim[i+2].integrate(...);
    acc += prim[i+3].integrate(...);
}
```
**Effort:** Low (compiler can do automatically with `#pragma unroll`)
**Expected gain:** 1.2-1.5x

---

## 6. OptiX Features

### optixReorder (Ray Coherence) - RECOMMENDED!

**Your setup benefits from this:**
- You have `samples_per_pixel > 1` (similar rays from same pixel)
- Secondary rays after scattering (benefit from reordering)
- Current execution model is compatible (no refactor needed)

**What it does:**
Reorders rays to improve coherence - groups similar rays together so they access similar BVH nodes and memory, improving cache hits.

**Implementation:**
```cuda
__raygen__rg() {
    // Setup ray
    auto ray = launch_params.camera_.jittered_ray(pixel_idx, jitter);

    // Reorder for coherence BEFORE tracing
    optixReorder(
        make_uint2(pixel_idx.x, pixel_idx.y),  // Coherence hint (pixel coords)
        2  // Number of hint dimensions
    );

    // Now trace (grouped with similar rays for better cache hits)
    trace_ch_collect(...);
}
```

**Expected gain:** 1.5-3x on BVH traversal time
**Effort:** Trivial (one line of code)
**Risk:** None (non-intrusive API call)

**Priority:** **Phase 1** - Try this immediately after basic implementation works!

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

## 8. Backface Culling + Exit Hit Computation (User Idea)

**Current:** Collect both entry AND exit hits for every primitive (2 hits per primitive)

**Your Idea:** Only collect entry hits, compute exit hits analytically

### Concept

For spherical Gaussians, if we know:
- Entry point at `t_entry`
- Ray direction `d`
- Gaussian center `c` and scale `s`

We can compute exit point analytically:
```cuda
// Collect only entries (cull backfaces during collection)
if (is_entry) {
    hit_buffer->emplace_back(t, prim_idx, false);
}
// Skip exits entirely

// Later, when processing:
for (hit : hit_buffer) {
    float t_entry = hit.t_hit;
    // Compute exit analytically (ray-sphere intersection math)
    float t_exit = compute_sphere_exit(ray, t_entry, prim_center, prim_scale);

    // Process entry->exit segment
    integrate(t_entry, t_exit);
}
```

### Implementation via Backface Culling

OptiX supports backface culling via ray flags:
```cuda
optixTrace(..., OPTIX_RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ...)
```

This would make OptiX skip all backfaces (exits), reporting only frontfaces (entries).

### Advantages
- **2x fewer hits to collect** - Only entries, not exits
- **2x less memory** - Half the buffer size needed
- **2x faster sorting** - Sorting 1000 vs 2000 hits
- **Simpler logic** - No need to track entry/exit pairs

### Challenges
- **Analytical exit computation** - Need to solve ray-sphere intersection for exit point
  - For transformed ellipsoids, this is non-trivial (need to account for rotation/scale)
  - Possible via quadratic formula in local space
- **Edge cases** - What if ray is tangent? What if entry is at grazing angle?
- **Numerical precision** - Computed exit might not match tessellated icosphere exit exactly

### Effort: Medium
- Enable backface culling in trace_ch_collect
- Implement analytical exit computation (ray-ellipsoid intersection solver)
- Update processing loop to generate exit hits on-the-fly
- Test for correctness vs current approach

### Expected Gain: 2-3x
- Half the hits to collect/sort/store
- Potential speedup if exit computation is cheaper than collection

### Risk: Medium
- Analytical computation might be expensive (erf, sqrt, etc.)
- Precision issues could cause artifacts
- Need to verify computed exits match tessellation closely

### Extension: OptiX Built-in Sphere Primitives

**Original problem with built-in spheres:**
You initially avoided OptiX's `OPTIX_PRIMITIVE_TYPE_SPHERE` because spheres don't report intersections when a ray originates **inside** them. This broke the ray-starts-inside case after scattering.

**With analytical exit computation, this becomes viable again!**

If we only need entry hits (ray entering from outside), OptiX built-in spheres work perfectly:
```cuda
// Use OPTIX_PRIMITIVE_TYPE_SPHERE instead of tessellated icospheres
// No custom geometry, no 5120 triangles per primitive
// OptiX hardware-accelerated sphere intersection

// Collect only entries (ray from outside hitting sphere)
trace_ch_collect(ray, 0, INF, &entries);

// Compute exits analytically (same as before)
for (entry : entries) {
    float t_exit = compute_sphere_exit(ray, entry.t, center, radius);
}
```

**Advantages over tessellated icospheres:**
- **No custom geometry** - Simpler asset pipeline
- **Smaller BVH** - Built-in spheres likely more compact than 5120 triangles
- **Faster intersection** - Hardware-optimized sphere test vs triangle tests
- **Less memory** - No vertex/index buffers for icospheres
- **Exact geometry** - No tessellation approximation artifacts

**Implementation:**
1. Change `OPTIX_PRIMITIVE_TYPE_FLAGS_TRIANGLE` → `OPTIX_PRIMITIVE_TYPE_FLAGS_SPHERE` in renderer.cpp
2. Remove icosphere mesh generation code
3. Build GAS with sphere primitives instead of triangle meshes
4. Enable backface culling + analytical exit computation

**Expected gain:** 3-10x
- Faster intersection tests (hardware spheres vs 5120 triangles)
- Smaller memory footprint
- Combined with 2x from only collecting entries

### Recommendation: Phase 2
Worth prototyping after basic collection works. Compare:
- Time to collect/sort 2N hits vs collect/sort N hits + compute N exits
- Accuracy of analytical exits vs tessellated geometry
- **Consider switching back to OptiX built-in spheres** (originally avoided, now viable)

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
