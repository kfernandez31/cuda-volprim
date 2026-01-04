# Next Optimization Tasks - Context for Future Sessions

**Date:** 2026-01-04
**Status:** Ready to begin optimization phase after stress test validation
**Current Branch:** `feature/test-suite`

---

## Overview

The renderer is now production-ready with:
- ✅ Batched online averaging (96.8% memory reduction)
- ✅ Complete test suite (36 validation + 6 stress tests)
- ✅ All geometric correctness tests passing
- ✅ Basic micro-optimizations complete (float4 alignment, loop unrolling, etc.)

**Next Phase:** Performance optimization guided by stress test profiling

---

## Immediate Tasks

### 1. Baseline Performance Measurement

Before any optimization work, collect baseline metrics:

```powershell
# Run all stress tests to get baseline timings
.\build\bin\Release\test_runner.exe --stress --spp=64 --output-dir=baseline_results

# Tests will render: 256, 512, 1024, 2048, 4096, 8192 gaussians
# Record: render time, memory usage, visual quality
```

**Expected outcomes:**
- Identify which gaussian counts cause performance degradation
- Establish baseline for measuring optimization impact
- Verify MAX_PRIMITIVES=128 is sufficient (should handle up to 8192 scene primitives)

---

## Priority Optimizations

### A. "Ray Started Inside" Optimization

**Current Problem (`device/core/sampling.cuh:278-288`):**

When processing exit hits, we branch on whether primitive is in `active_prims`:

```cuda
if (active_prims.contains(hit.prim_idx)) {
    // Normal paired exit
    active_prims.erase(hit.prim_idx);
} else {
    // Ray started inside this primitive (no entry was detected)
    // This happens when scatter event placed ray origin INSIDE a primitive
    // Manually integrate optical depth from ray origin (t=0) to this exit
    on_ray_started_inside(hit.prim_idx, t_exit);
}
```

**Proposed Optimization:**

Pre-populate `active_prims` at the start of each bounce with all primitives containing the ray origin:

```cuda
// At start of collect_and_sort_hits() in each bounce:
void collect_and_sort_hits(
    const geometry::Ray& ray,
    utils::Set<size_t, consts::ACTIVE_PRIMS_CAPACITY>& active_prims,
    // ... other params
) {
    // NEW: Pre-populate active_prims with primitives containing ray origin
    for (size_t i = 0; i < num_primitives; ++i) {
        if (point_inside_ellipsoid(ray.origin, primitives[i])) {
            active_prims.insert(i);
        }
    }

    // Rest of function unchanged - now all exits are "normal" exits
    // No more branching on exit hits!
}
```

**Benefits:**
- **Eliminates branch divergence** on exit hits (always `contains() == true`)
- **Simpler logic:** Single code path for all exit hits
- **Cleaner code:** Remove `on_ray_started_inside` callback entirely

**Costs:**
- O(N) loop per bounce (N = total scene primitives)
- Point-in-ellipsoid test per primitive: ~50-100 cycles each

**When Worth It:**
- **N ≤ 64 primitives:** Always worth it (~3000-6000 cycles vs. warp divergence cost)
- **64 < N ≤ 256:** Profile to measure impact
- **N > 256:** Likely not worth it (15k+ cycles), consider spatial acceleration instead

**Implementation Location:**
- Modify: `device/core/sampling.cuh` - `collect_and_sort_hits()` function
- Remove: `on_ray_started_inside` callback from `process_hit_clusters()`
- Simplify: Exit hit handling logic (unconditional erase)

**Files to Change:**
1. `device/core/sampling.cuh` - Add containment loop, remove branch
2. `device/core/intersection.cuh` - May need `point_inside_ellipsoid()` helper
3. `device/core/trace.cuh` - Update if containment check needed

---

### B. Priority 3: Profile-Driven Optimizations (from CLAUDE.md)

**These optimizations should ONLY be applied after profiling stress tests**

| Optimization | When to Apply | Expected Gain | Effort |
|-------------|---------------|---------------|--------|
| Warp-level optical depth | If integration >30% of total time | 1.5-2× | Medium |
| Early termination (batched sort) | If most scattering occurs in first few hits | 3-10× | Medium |
| Global memory pool | If stack pressure limits occupancy | Better occupancy | Medium |
| Warp-cooperative hit collection | If BVH traversal >50% of time | 5-20× | Very High |

**Profiling Commands:**

```bash
# Timeline view (identify overall bottlenecks)
nsys profile --stats=true --output=profile_stress_8192 ./build/bin/Release/test_runner.exe --scene=stress_8192_gaussians --spp=64

# Kernel-level analysis (drill into expensive kernels)
ncu --set full --output=kernel_stress_8192 ./build/bin/Release/test_runner.exe --scene=stress_8192_gaussians --spp=64

# Occupancy check
ncu --metrics sm__warps_active.avg.pct_of_peak_sustained_active ./build/bin/Release/test_runner.exe --scene=stress_8192_gaussians --spp=64
```

**Analysis Targets:**

Look for:
- **BVH Traversal time:** If >50% → consider warp-cooperative hit collection
- **Optical depth integration time:** If >30% → consider warp-level reduction
- **Sorting time:** If >15% → consider batched sort with early termination
- **Occupancy:** If <50% → likely stack pressure, consider memory pool

**Expected Baseline Distribution:**
```
BVH Traversal (OptiX trace):     40-60%
Optical Depth Integration:        20-30%
Sorting:                          5-15%
Random Number Generation:         5-10%
Memory Operations:                5-10%
```

If actual distribution differs significantly, prioritize accordingly.

---

### C. Priority 4: Major Refactors (from CLAUDE.md)

**⚠️ ONLY pursue if profiling shows clear bottleneck justifying the effort**

| Optimization | Condition | Potential Gain | Effort |
|-------------|-----------|----------------|--------|
| Warp-cooperative execution model | BVH traversal >50% of time | 5-30× | Very High |
| Warp-cooperative sorting | After execution model change | 2-4× | Very High |

**Warp-Cooperative Model:**

Current model: **One Thread = One Ray**
```
32 threads → 32 independent rays → warp divergence
```

Alternative model: **One Warp = One Ray**
```
32 threads → 1 shared ray → parallel hit processing
```

**Benefits:**
- Parallel hit collection across warp
- Parallel optical depth integration
- Parallel sorting (warp-level primitives)
- Eliminates bounce-level divergence

**Costs:**
- Complete architecture redesign (~1000+ lines changed)
- Complex shared memory management
- Harder to debug and maintain

**Recommendation:**
- Only pursue if stress tests show >50% time in BVH traversal
- Requires profiling to justify effort
- Consider as "Phase 2" optimization after exhausting simpler approaches

---

## Implementation Strategy

### Step 1: Measure Baseline ✅ Ready
```powershell
.\build\bin\Release\test_runner.exe --stress --spp=64
```

### Step 2: "Ray Started Inside" Optimization (Low Risk, High Reward)
- Implement containment pre-computation
- Benchmark on `stress_8192_gaussians` (worst case)
- If >2% slowdown for N>256, make it conditional on primitive count

### Step 3: Profile Stress Tests
```bash
# Profile worst-case scenario
nsys profile --output=profile_8192 ./test_runner.exe --scene=stress_8192_gaussians --spp=64
```

### Step 4: Apply Targeted Optimizations
- Based on profiling, implement ONE Priority 3 optimization at a time
- Measure impact before proceeding to next
- Document performance gains

### Step 5: Re-evaluate
- If gains insufficient and profiling justifies it → consider Priority 4
- Otherwise, declare optimization phase complete and move to Jorge asset validation

---

## Important Notes

### Don't Over-Optimize
- Renderer is already production-ready
- Focus on optimizations that provide >10% improvement
- Avoid premature optimization without profiling data

### Maintain Correctness
- After each optimization, run full test suite:
  ```powershell
  .\build\bin\Release\test_runner.exe --validation --spp=64
  ```
- Visual inspection of output images
- Ensure no regressions in geometric correctness

### Git Workflow
- Create feature branch for each optimization (e.g., `feature/ray-inside-optimization`)
- Commit with detailed performance measurements
- Merge only if improvement is measurable and tests pass

---

## Current System Specs (for Profiling Reference)

**Hardware:**
- GPU: RTX 2080 (8 GB VRAM)
- CUDA Compute Capability: 7.5
- SM Count: 46
- Max threads per SM: 1024

**Build Configuration:**
- Release build (optimizations enabled)
- THESIS_NUMERICAL_GUARDS: Disabled in Release
- MAX_PRIMITIVES: 128 (supports up to ~8k scene primitives)
- HIT_BUFFER_CAPACITY: 256 (2 × MAX_PRIMITIVES)

---

## Key Code Locations

### For "Ray Started Inside" Optimization:
- **Main logic:** `device/core/sampling.cuh` - `collect_and_sort_hits()` function
- **Exit handling:** `device/core/sampling.cuh:278-288` - branch to eliminate
- **Intersection helper:** `common/geometry/intersection.h` - may need `point_inside_ellipsoid()`

### For Profiling:
- **Ray generation:** `device/entry/raygen.cuh:20-98` - main path tracing loop
- **Optical depth:** `device/core/sampling.cuh` - `compute_optical_depth_along_ray()`
- **Sorting:** `device/core/sorting.cuh` - adaptive hybrid sort
- **Trace:** `device/core/trace.cuh` - OptiX trace wrapper

---

## Success Criteria

**Phase Complete When:**
1. ✅ Baseline stress test timings recorded
2. ✅ "Ray started inside" optimization implemented and benchmarked
3. ✅ Profiling completed on all 6 stress tests
4. ✅ At least 2 Priority 3 optimizations evaluated (implement if >10% gain)
5. ✅ Full test suite still passing
6. ✅ Performance improvements documented

**Then:** Move to Jorge asset validation (CLAUDE.md Section 7)

---

## References

- Main documentation: `CLAUDE.md` (Section 6: Optimization Roadmap)
- Detailed GPU optimizations: `plans/OPTIMIZATION_TODO.md`
- Test suite: `test/scenes/geometric_validation.cpp`
- Constants: `device/core/constants.cuh`
