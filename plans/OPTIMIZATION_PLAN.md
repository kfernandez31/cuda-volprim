# Optimization Implementation Plan
**Date:** 2026-01-04
**Status:** Ready to Execute
**Branch:** `feature/test-suite` → merge to `main`, then create optimization branches

---

## Executive Summary

### Profiling Results (stress_1024_gaussians @ RTX 3090)

**Critical Finding: Register Pressure Limits Occupancy**
- **Achieved Occupancy:** 32.63% ⚠️ (Target: >60%)
- **Registers/Thread:** 124 (High - caused by stack-allocated buffers)
- **Active Warps/SM:** 15.66 / 48 (only 32.6% of capacity)
- **Memory Throughput:** 55% (balanced, not bottleneck)
- **Compute Throughput:** 55% (balanced, not bottleneck)

**Root Cause Analysis:**
```
Stack allocations per thread:
├─ Hit buffer: 256 hits × 24 bytes = 6,144 bytes
├─ Active prims: 256 × 4 bytes = 1,024 bytes
├─ Ray state, RNG, loop vars = ~500 bytes
└─ Total: ~7.7KB per thread → 124 registers consumed

Low occupancy → Cannot hide memory latency → Throughput capped at 55%
```

**Verdict:** Profiling **directly confirms** Priority 3 "Global memory pool" and Priority 4 "Warp-cooperative" optimizations from CLAUDE.md.

---

## Optimization Roadmap

### Phase 1: Quick Win (5 minutes, 1.2× expected) ✅ COMPLETED

**Action:** Reduce MAX_PRIMITIVES from 256 → 128

**Already done** (in constants.cuh line 32). Benefits:
- Smaller buffers → fewer registers (124 → ~80 expected)
- Occupancy improvement: 32% → 40-45%
- Speedup: ~1.2×

**Validation:**
- Re-run profiling to confirm occupancy improvement
- Ensure stress tests still pass (especially 8192 gaussians)

---

### Phase 2: Global Memory Pool (HIGH PRIORITY, LOW RISK) 🎯

**⚠️ REVISED: Start with simple solution before complex warp-cooperative model**

**Why Global Memory Pool First:**
1. ✅ **Simple:** Keep "one thread = one ray" model (no warp divergence issues)
2. ✅ **Low Risk:** 1-2 days implementation vs 2-3 weeks for warp-cooperative
3. ✅ **Effective:** Frees 70-80 registers → 55-65% occupancy expected
4. ✅ **Safe:** Easy to debug, easy to revert if issues arise

**Expected Impact:**
- Registers/thread: 124 → 40-50 (freed ~70-80 registers!)
- Occupancy: 32% → 55-65%
- Speedup: **1.5-2.0×**

**Critical Advantage over Warp-Cooperative:**
- **No warp divergence problem:** Each thread still processes one ray independently
- If Ray A terminates at bounce 3 and Ray B needs 10 bounces, both threads work independently
- Warp-cooperative would idle entire warps when one ray terminates early

**Architecture Change:**

```
CURRENT: One Thread = One Ray (stack-allocated buffers)
├─ Thread 0: Ray 0 [stack: Hit[256], Active[256]] → 124 registers
├─ Thread 1: Ray 1 [stack: Hit[256], Active[256]] → 124 registers
└─ ...
Result: 32% occupancy (register pressure)

GLOBAL POOL: One Thread = One Ray (global memory buffers)
├─ Thread 0: Ray 0 [pointer to global pool] → ~40-50 registers
├─ Thread 1: Ray 1 [pointer to global pool] → ~40-50 registers
└─ ...
Result: 55-65% occupancy (no register pressure)
```

**Memory Layout:**
```cuda
// Host allocates once per render
struct LaunchParams {
    Hit* hit_buffer_pool_;        // [width × height × MAX_PRIMITIVES × 2]
    uint* active_prims_pool_;     // [width × height × MAX_PRIMITIVES]
    // ... existing fields
};

// In renderer initialization:
size_t num_pixels = width * height;
cudaMalloc(&hit_buffer_pool_, num_pixels * 512 * sizeof(Hit));
cudaMalloc(&active_prims_pool_, num_pixels * 256 * sizeof(uint));

// Device: Each thread gets its slice
__global__ void __raygen__rg() {
    const size_t pixel_idx = launch_idx.y * launch_params.image_.width_ + launch_idx.x;

    // Get pointers to this pixel's buffers
    Hit* my_hits = &launch_params.hit_buffer_pool_[pixel_idx * 512];
    uint* my_prims = &launch_params.active_prims_pool_[pixel_idx * 256];

    // Wrap in existing data structures (minimal code changes!)
    HitBuffer hit_buffer(my_hits, 512);
    PrimsSet active_prims(my_prims, 256);

    // Rest of raygen logic UNCHANGED
}
```

**Implementation Plan:**

#### Step 2.1: Update Data Structures to Support Global Memory
- **Branch:** `feature/global-memory-pool`
- **Files to Modify:**
  - `include/thesis/device/utils/vector.h` - Already has `DynamicVector` support!
  - `include/thesis/device/utils/set.h` - Check if supports external storage

**Verify existing DynamicVector/DynamicStorage works:**
```cpp
// Check vector.h - should already support this pattern:
template <typename T>
using DynamicVector = VectorBase<T, DynamicStorage<T>>;

// DynamicStorage constructor:
DynamicStorage(T* ptr, size_t cap) : capacity_(cap), data_(ptr) {}
```

#### Step 2.2: Add Pools to LaunchParams
- **Files:**
  - `include/thesis/common/params/launch_params.h` - Add pool pointers
  - `device/core/launch_params.cuh` - Device-side declaration

**Changes:**
```cpp
struct LaunchParams {
    // ... existing fields
    device::Hit* hit_buffer_pool_;       // NEW
    uint* active_prims_pool_;            // NEW
    // ... rest
};
```

#### Step 2.3: Allocate Pools in Renderer
- **File:** `src/thesis/host/app/renderer.cpp`
- **Location:** In constructor or `initPipeline()`

**Implementation:**
```cpp
void Renderer::initBufferPools() {
    size_t num_pixels = config_.image_width_ * config_.image_height_;

    // Allocate hit buffer pool
    size_t hit_pool_size = num_pixels * consts::HIT_BUFFER_CAPACITY;
    CUDA_CHECK(cudaMalloc(&hit_buffer_pool_, hit_pool_size * sizeof(Hit)));

    // Allocate active prims pool
    size_t prims_pool_size = num_pixels * consts::MAX_PRIMITIVES;
    CUDA_CHECK(cudaMalloc(&active_prims_pool_, prims_pool_size * sizeof(uint)));

    // Store in launch params
    launch_params_.hit_buffer_pool_ = hit_buffer_pool_;
    launch_params_.active_prims_pool_ = active_prims_pool_;
}

// Don't forget to free in destructor
Renderer::~Renderer() {
    if (hit_buffer_pool_) cudaFree(hit_buffer_pool_);
    if (active_prims_pool_) cudaFree(active_prims_pool_);
}
```

#### Step 2.4: Modify Raygen to Use Pools
- **File:** `device/entry/raygen.cuh`
- **Change:** Replace stack arrays with pointers to pool

**Before:**
```cuda
__global__ void __raygen__rg() {
    // ... existing setup

    optix::ScatteringEvent<consts::ACTIVE_PRIMS_CAPACITY> event;
    // event.active_prims_ is StaticSet (stack allocated)
```

**After:**
```cuda
__global__ void __raygen__rg() {
    const auto launch_idx = optixGetLaunchIndex();
    const size_t pixel_idx = launch_idx.y * launch_params.image_.width_ + launch_idx.x;

    // Get this thread's slice of global pools
    uint* my_prims_data = &launch_params.active_prims_pool_[pixel_idx * consts::MAX_PRIMITIVES];

    // Initialize event with dynamic storage
    optix::ScatteringEvent event;
    event.active_prims_.init_from_external_storage(my_prims_data, consts::MAX_PRIMITIVES);
    // ... rest unchanged
```

**Also modify sampling.cuh:**
```cuda
// In collect_and_sort_hits() and sample_scattering_event()
// Replace: HitBuffer hit_buffer;  // Stack
// With:    HitBuffer hit_buffer(pool_ptr, capacity);  // Global
```

#### Step 2.5: Update ScatteringEvent Structure
- **File:** `include/thesis/device/optix/scattering_event.h` (if exists)
- **Change:** Support external storage for active_prims

**Modify to accept external storage:**
```cpp
template <size_t Capacity>
struct ScatteringEvent {
    utils::Set<uint, Capacity> active_prims_;  // Or use DynamicSet
    // ...

    // Add initialization method
    void init_with_external_storage(uint* storage, size_t cap) {
        active_prims_ = utils::Set<uint>(storage, cap);
    }
};
```

#### Step 2.6: Testing & Validation
- Run full validation suite: `--validation --spp=64`
- Run stress tests: `--stress --spp=64`
- Profile to confirm occupancy improvement
- Benchmark speedup vs. baseline

**Success Criteria:**
- ✅ All tests pass (visual + numerical correctness)
- ✅ Occupancy >55%
- ✅ Speedup >1.5× on stress_1024_gaussians

---

### Phase 3: Profile Divergence & Decide on Warp-Cooperative

**⚠️ ONLY pursue warp-cooperative if global memory pool is insufficient**

#### Step 3.1: Profile Warp Divergence
**Before** considering warp-cooperative, measure branch divergence:

```bash
sudo /usr/local/cuda/bin/ncu --metrics \
  sm__sass_branch_targets_threads_diverged.avg.pct_of_peak_sustained_active,\
  smsp__average_warp_latency_issue_stall_short_scoreboard.pct \
  -o divergence_profile ./build/bin/test_runner --scene=stress_1024_gaussians --spp=64
```

**Decision Criteria:**
- Divergence <20% AND occupancy still <50% → Warp-cooperative worth considering
- Divergence >30% → **DON'T do warp-cooperative** (will make it worse!)
- Occupancy >60% after global pool → **No need**, optimization complete ✓

#### Step 3.2: Profile Other Stress Tests

**Goal:** Validate scaling behavior across different scene complexities

**Tests to profile:**
```bash
sudo /usr/local/cuda/bin/ncu --set basic -o profile_256 \
    ./build/bin/test_runner --scene=stress_256_gaussians --spp=64

sudo /usr/local/cuda/bin/ncu --set basic -o profile_4096 \
    ./build/bin/test_runner --scene=stress_4096_gaussians --spp=64

sudo /usr/local/cuda/bin/ncu --set basic -o profile_8192 \
    ./build/bin/test_runner --scene=stress_8192_gaussians --spp=64
```

**Analysis:**
- Confirm occupancy scales consistently
- Identify if any new bottlenecks emerge at high primitive counts
- Validate MAX_PRIMITIVES=128 is sufficient

---

### Phase 4: Warp-Cooperative Model (ONLY IF DIVERGENCE IS LOW)

**⚠️ HIGH RISK, HIGH COMPLEXITY - Only pursue if all conditions met:**
1. Global memory pool achieved <50% occupancy
2. Divergence profiling shows <20% branch divergence
3. You have 2-3 weeks available for implementation
4. Jorge asset validation can wait

**Key Challenge: Warp Divergence**
```
Problem: Rays terminate at different bounce counts
- Ray A: 3 bounces (Russian roulette terminates early)
- Ray B: 10 bounces (still scattering)

In warp-cooperative model (32 threads = 1 ray):
- When Ray A finishes, ALL 32 threads exit
- Those threads sit idle while Ray B's warp continues
- Result: Potentially WORSE performance due to idle warps
```

**When It Makes Sense:**
- Scenes with consistent bounce depths (low variance)
- After measuring divergence <20% via profiling
- Need for >2× speedup over global pool

**Architecture:** See original plan's Phase 2 warp-cooperative details

**Expected Impact (if conditions met):**
- Occupancy: 55% → 70-80%
- Speedup: **1.5-2.0× over global pool** (3-4× total)
- Risk: Very High (could make performance worse)

**Recommendation:** Unlikely to be needed. Global memory pool should be sufficient.

---

## Implementation Timeline (REVISED)

### Week 1: Global Memory Pool (LOW RISK PATH)
- **Day 1:** Verify data structure support (DynamicVector/DynamicSet)
- **Day 2:** Add pools to LaunchParams, allocate in Renderer
- **Day 3:** Modify raygen.cuh and sampling.cuh to use pools
- **Day 4:** Testing, debugging, validation suite
- **Day 5:** Profile and benchmark, compare to baseline

**Total effort:** 3-5 days for global memory pool

### Week 2: Evaluation & Next Steps
- **Day 1:** Profile warp divergence
- **Day 2:** Profile all stress tests (256, 1024, 4096, 8192)
- **Day 3:** Decision point: Is further optimization needed?
  - If occupancy >60% → Done! Move to Jorge asset ✓
  - If occupancy <50% AND divergence <20% → Consider warp-cooperative
  - Otherwise → Document findings, move to Jorge asset

### Week 3+: Warp-Cooperative (ONLY IF NEEDED)
- **Only pursue if:**
  - Global pool isn't enough AND
  - Divergence profiling looks favorable (<20%) AND
  - You have 2-3 weeks available
- **Complexity:** Very High (see Phase 4 details below)

---

## Risk Assessment

### High Risk Items
1. **Warp divergence in bounce loop:** Rays terminate at different bounces
   - **Mitigation:** Use warp-level voting, early exit gracefully

2. **Shared memory bank conflicts:** Parallel access patterns
   - **Mitigation:** Careful layout planning, padding if needed

3. **Complexity explosion:** Debugging warp-level bugs is hard
   - **Mitigation:** Incremental development, extensive testing

### Fallback Strategy
If warp-cooperative hits major blockers:
1. Keep infrastructure (warp primitives are reusable)
2. Fall back to global memory pool approach
3. Still achieve 1.4-1.6× speedup (better than nothing)

---

## Success Metrics

**Minimum Acceptable:**
- Occupancy: 32% → >50%
- Speedup: >1.5× on stress_1024_gaussians
- All validation tests pass

**Target:**
- Occupancy: 32% → >60%
- Speedup: >2× on stress_1024_gaussians
- Speedup scales to 8192 gaussians

**Stretch Goal:**
- Occupancy: >75%
- Speedup: >3× on all stress tests
- Ready for Jorge asset at 4K resolution

---

## References

- **Profiling data:** `profile_kernel_1024.ncu-rep`
- **Architecture docs:** `CLAUDE.md` Section 2, 6
- **Test suite:** `test/scenes/geometric_validation.cpp`
- **Constants:** `device/core/constants.cuh`
- **Warp primitives guide:** CUDA C++ Programming Guide, Chapter 10

---

## Git Strategy

```bash
# Merge current work
git checkout main
git merge feature/test-suite
git branch -d feature/test-suite

# Create optimization branches
git checkout -b feature/warp-cooperative-base
# ... implement infrastructure
git checkout main
git merge feature/warp-cooperative-base

git checkout -b feature/warp-cooperative-hits
# ... implement parallel ops
# ... test and validate
git checkout main
git merge feature/warp-cooperative-hits

# Tag release
git tag v1.0-warp-optimized
```

---

## Next Steps (Immediate Actions)

### Step 1: Merge Current Work ✅
```bash
git add .
git commit -m "Add ray-starts-inside optimization and refactor stress tests

- Pre-populate active_prims with primitives containing ray origin
- Eliminates branch divergence in exit hit processing
- Refactor stress tests to use helper function (reduce code duplication)
- MAX_PRIMITIVES set to 256 for profiling baseline

Profiling results (stress_1024_gaussians @ RTX 3090):
- Occupancy: 32.63% (register pressure from stack buffers)
- Registers/thread: 124
- Memory/Compute balanced at ~55%
- Clear bottleneck: Stack-allocated buffers consuming registers"

git checkout main
git merge feature/test-suite
git branch -d feature/test-suite
git tag v0.9-baseline-profiled
```

### Step 2: Create Optimization Branch
```bash
git checkout -b feature/global-memory-pool
```

### Step 3: Implement Global Memory Pool (3-5 days)

**Follow Phase 2 Implementation Plan:**
1. Verify `DynamicVector` and `DynamicSet` support external storage
2. Add pool pointers to `LaunchParams`
3. Allocate pools in `Renderer::initBufferPools()`
4. Modify `raygen.cuh` to get per-pixel slices
5. Modify `sampling.cuh` to use dynamic buffers
6. Test and validate

**Estimated memory usage:**
- 1920×1080 × 256 × 2 × 24 bytes (hit buffer) = 2.3 GB
- 1920×1080 × 256 × 4 bytes (active prims) = 2.1 GB
- **Total: ~4.4 GB** (fits on RTX 3090 with 24GB)

### Step 4: Profile and Evaluate
```bash
# After implementation, profile again
sudo /usr/local/cuda/bin/ncu --set basic -o profile_after_pool \
  ./build/bin/test_runner --scene=stress_1024_gaussians --spp=64

# Compare occupancy (target: >55%)
ncu --import profile_after_pool.ncu-rep | grep "Achieved Occupancy"
```

### Step 5: Decision Point
- **If occupancy >60%:** Merge, tag `v1.0-optimized`, move to Jorge asset ✓
- **If occupancy 50-60%:** Acceptable gains, move to Jorge asset ✓
- **If occupancy <50%:** Profile divergence, consider warp-cooperative

---

## How to Proceed (TL;DR)

1. **Commit and merge current work** (ray-starts-inside optimization)
2. **Create `feature/global-memory-pool` branch**
3. **Implement global memory pool** (follow Phase 2 plan)
4. **Profile to validate improvement** (target: 1.5-2× speedup)
5. **If sufficient, move to Jorge asset validation**
6. **Only if needed, consider warp-cooperative** (after divergence profiling)

**Start with:** Global memory pool (low risk, high reward) 🎯
