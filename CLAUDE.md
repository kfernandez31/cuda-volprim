# Gaussian Volumetric Path Tracer

**Project:** OptiX + CUDA Physically Based Volumetric Renderer
**Target:** Production-quality single-frame renderer for Gaussian volumetric primitives
**Validation:** Comparison against Mitsuba reference implementation
**Status:** Core algorithm complete, entering optimization and productization phase

---

## Table of Contents

0. [Development Guidelines](#0-development-guidelines)
1. [Executive Summary](#1-executive-summary)
2. [Architecture Overview](#2-architecture-overview)
3. [Production Rendering: Memory Architecture](#3-production-rendering-memory-architecture)
4. [Testing Framework](#4-testing-framework)
5. [Profiling Strategy](#5-profiling-strategy)
6. [Optimization Roadmap](#6-optimization-roadmap)
7. [Mitsuba Validation](#7-mitsuba-validation)
8. [Future Enhancements](#8-future-enhancements)
9. [Implementation Schedule](#9-implementation-schedule)

---

## 0. Development Guidelines

### Git Workflow

**Branch Strategy:**
- `main` is the single source of truth - always stable and working
- One feature branch per feature/change
- Branch naming: `feature/descriptive-name` (e.g., `feature/batched-rendering`)
- After merge, delete the feature branch immediately

**Commit Process:**
1. Make changes on feature branch
2. Stage relevant files only (exclude unrelated changes)
3. Run three parallel review agents:
   - **Code quality agent**: Check for bugs, memory leaks, race conditions
   - **Style agent**: Verify modern C++ practices, naming conventions
   - **Testing agent**: Identify what needs testing
4. User reviews agent feedback and code diff
5. User approves commit contents explicitly
6. Create commit with descriptive message (NO AI tool mentions)
7. Merge to main via fast-forward or standard merge
8. Delete feature branch

**Commit Message Format:**
```
Brief summary of change (imperative mood)

Detailed explanation of why this change was made.
Include performance implications, memory changes, etc.

Key changes:
- Bullet point 1
- Bullet point 2
```

### Code Quality Standards

**Modern C++ Practices:**
- Use `std::string_view` instead of `const std::string&` for read-only string parameters
- Use `std::span` for array views instead of pointer + size
- Prefer `auto` for type deduction when type is obvious
- Use structured bindings: `auto [x, y] = get_pair();`
- Use `if constexpr` for compile-time branches
- Prefer `[[nodiscard]]` for functions that return important values
- Use `std::optional` instead of sentinel values or output parameters
- Prefer range-based for loops: `for (const auto& item : container)`

**CUDA/OptiX Specific:**
- Mark device functions with `__device__ __forceinline__` for performance
- Use `__restrict__` for non-aliasing pointers in kernels
- Prefer `float3` over `make_float3` in hot paths (register pressure)
- Document memory layout (coalescing strategy) in comments
- Always check `CUDA_CHECK` and `OPTIX_CHECK` for API calls

**Memory Safety:**
- No raw `new`/`delete` - use RAII wrappers
- No manual memory management - use smart pointers or custom RAII types
- Document ownership semantics in comments
- Use `const` wherever possible

**Performance:**
- Document Big-O complexity for non-trivial algorithms
- Profile before optimizing - use NSight Systems/Compute
- Prefer simplicity over premature optimization
- Document performance-critical sections

**Testing Requirements:**
- Every new feature needs test cases (see Section 4)
- Validate against reference implementation when applicable
- Check edge cases: empty inputs, boundary conditions, large datasets

---

## 1. Executive Summary

### What This Project Is

A **physically based volumetric path tracer** rendering participating media represented by **Gaussian ellipsoids**. Each primitive is a 3D Gaussian density field bounded by tessellated icospheres. The renderer integrates optical depth analytically using error functions and uses Monte Carlo sampling for scattering events.

### Current Capabilities

| Feature | Status | Notes |
|---------|--------|-------|
| Batched online averaging | ✅ Complete | Enables 4K @ 1024+ spp, 96.8% memory reduction |
| Anyhit buffer-based hit collection | ✅ Complete | Solves coincident surface problem |
| Analytical exit computation | ✅ Complete | Sphere equation in local space |
| Transform support (TRS) | ✅ Complete | Scale, rotation (quaternions), translation |
| Adaptive hybrid sorting | ✅ Complete | Insertion (n≤64) + bitonic (n>64) |
| GLM dependency removed | ✅ Complete | Native CUDA types throughout |
| Camera-inside detection | ✅ Complete | Host-side pre-computation |
| Environment map lighting | ✅ Complete | HDR environment maps |
| Russian roulette termination | ✅ Complete | Path termination strategy |

### Critical Blocker ✅ SOLVED

**Memory wall (SOLVED):** Previous architecture allocated `O(pixels × samples)` GPU memory, making production renders impossible.

**Previous limitations:**
| Resolution | SPP | Memory Required | RTX 2080 (8GB) |
|------------|-----|-----------------|----------------|
| 1920×1080 | 207 | 6.8 GB | Max stable |
| 1920×1080 | 1024 | 34 GB | ❌ Impossible |
| 3840×2160 | 1024 | 135 GB | ❌ Impossible |

**Solution implemented:** Batched online averaging reduces memory to `O(pixels)`:
| Resolution | SPP | Memory Required | RTX 2080 (8GB) |
|------------|-----|-----------------|----------------|
| 1920×1080 | 1024 | 1.1 GB | ✅ Possible |
| 3840×2160 | 1024 | 4.2 GB | ✅ Possible |
| 3840×2160 | 4096 | 4.2 GB | ✅ Possible |

**Performance impact:** Negligible (~2ms launch overhead, near-perfect linear scaling)

### Path Forward

```
Phase A: Productization (1-2 days)
  └─ Implement batched rendering → Enables 4K @ 1024+ spp

Phase B: Testing (2-3 days)
  └─ Geometric validation suite → Verify correctness

Phase C: Profiling (1 day)
  └─ Identify computational bottlenecks → Guide optimization

Phase D: Optimization (profile-driven)
  └─ Apply targeted optimizations → Performance gains

Phase E: Validation (2-3 days)
  └─ Mitsuba comparison → Quality metrics (PSNR, SSIM)
```

---

## 2. Architecture Overview

### Rendering Model

```
Ray Generation
     │
     ▼
┌────────────────────────────────────────┐
│  OptiX Trace (Anyhit Collection)       │
│  - Single trace per ray                │
│  - Anyhit collects ALL entry hits      │
│  - Backface culling (entry faces only) │
│  - Closesthit disabled                 │
└────────────────────────────────────────┘
     │
     ▼
┌────────────────────────────────────────┐
│  Hit Processing                        │
│  - Analytical exit computation         │
│  - Adaptive hybrid sort (by t-value)   │
│  - Optical depth integration (erf)     │
│  - Monte Carlo scattering sampling     │
└────────────────────────────────────────┘
     │
     ▼
┌────────────────────────────────────────┐
│  Scattering or Escape                  │
│  - τ = -ln(1-χ) sampling               │
│  - Bisection search for exact t        │
│  - Russian roulette termination        │
│  - Environment map lookup on escape    │
└────────────────────────────────────────┘
```

### Code Structure

```
thesis/
├── device/                         # GPU code
│   ├── entry/
│   │   ├── raygen.cuh              # Main path tracing loop
│   │   ├── anyhit.cuh              # Hit collection into buffer
│   │   └── miss.cuh                # Environment lookup
│   ├── core/
│   │   ├── sampling.cuh            # Scattering, optical depth
│   │   ├── trace.cuh               # OptiX trace wrappers
│   │   ├── sorting.cuh             # Adaptive hybrid sort
│   │   └── constants.cuh           # MAX_CAPACITY, epsilon values
│   ├── kernels/
│   │   └── average_samples.cu      # Sample averaging kernel
│   └── params/
│       └── primitive.h             # Device-side primitive
├── include/thesis/host/            # Host headers
│   ├── app/
│   │   ├── renderer.h              # Main renderer class
│   │   └── config.h                # CLI configuration
│   ├── cuda/
│   │   ├── async_buffer.h          # Async transfers (pinned memory)
│   │   └── stream_dag.h            # Stream dependency management
│   ├── optix/
│   │   └── *.h                     # Pipeline, GAS, IAS, SBT
│   └── params/
│       ├── image.h                 # Image buffer management
│       └── primitive.h             # Host-side primitive
├── src/thesis/host/                # Host implementation
│   ├── app/
│   │   └── renderer.cpp            # Renderer implementation
│   └── utils/
│       └── io.cpp                  # PLY loading, EXR export
└── common/                         # Shared host/device
    ├── geometry/
    │   ├── intersection.h          # Ray-sphere intersection
    │   └── quat.h                  # Quaternion math
    └── utils/
        └── math.h                  # Vector utilities
```

### Execution Model

**Current:** One Thread = One Ray (independent processing)

```
Pixel (x, y) with N samples:
├── Thread 0: Ray 0 → full path → contribution
├── Thread 1: Ray 1 → full path → contribution
├── ...
└── Thread N-1: Ray N-1 → full path → contribution
                              ↓
                    Average all contributions
```

**Characteristics:**
- No inter-thread communication during ray processing
- Warp lanes process 32 different rays simultaneously
- Simple to reason about, easy to debug
- Warp divergence at high bounce counts (RR termination)

---

## 3. Production Rendering: Memory Architecture

### The Problem

Current implementation uses OptiX's 3D launch grid `[width, height, samples]` with a sample buffer of size `pixels × samples × 16 bytes`. This creates a **hard memory ceiling**:

```cpp
// Current allocation (PROBLEMATIC)
sample_buffer_managed_(width * height * num_samples_per_pixel, ...)
// 1920×1080×1024×16 = 33.6 GB (impossible on consumer GPUs)
```

**Empirical findings on RTX 2080 (8 GB):**
- Max stable at 1080p: ~207 samples per pixel
- Beyond 207 spp: OOM errors, screen flickering
- 4K rendering: impossible at any meaningful spp

### The Solution: Batched Online Averaging

**Industry-standard pattern** used by Mitsuba, Arnold, Cycles, and OptiX samples:

```cpp
// NEW: Sample-independent memory allocation
float3* accumulator = allocate(width * height);        // ~24 MB for 4K
float4* batch_buffer = allocate(width * height * BATCH_SIZE);  // ~1 GB

for (int batch = 0; batch < num_batches; ++batch) {
    // Render batch of samples
    raygen_kernel<<<grid>>>(batch_buffer, batch * BATCH_SIZE);

    // Accumulate into running sum (single non-atomic write per pixel)
    accumulate_kernel<<<grid>>>(accumulator, batch_buffer, batch_size);

    // Optional: progressive preview
    if (should_save_preview) save_preview(accumulator, samples_so_far);
}

// Final output
normalize_and_save(accumulator, total_samples);
```

**Key insight:** No atomics needed. Each thread owns exactly one pixel. Multiple samples processed sequentially within each thread, accumulated in registers.

### Memory Comparison

| Configuration | Current | Batched (batch=64) | Reduction |
|---------------|---------|-------------------|-----------|
| 1080p × 1024 spp | 34 GB | 1.1 GB | **96.8%** |
| 4K × 1024 spp | 135 GB | 4.2 GB | **96.9%** |
| 4K × 4096 spp | 540 GB | 4.2 GB | **99.2%** |

### Implementation Requirements

**1. New device structures:**

```cpp
// device/params/image.h
struct Image {
    float3* accumulator_;        // Running sum [pixels]
    float4* batch_buffer_;       // Current batch [batch_size × pixels]
    int batch_offset_;           // Current batch start index
    int batch_size_;             // Samples in this batch
    int total_spp_;              // Total samples for normalization
};
```

**2. Modified raygen kernel:**

```cpp
__global__ void __raygen__rg() {
    const uint3 idx = optixGetLaunchIndex();
    const int pixel_idx = idx.y * params.width + idx.x;

    // Accumulate batch_size samples in registers
    float3 batch_contribution = make_float3(0.0f);
    for (int s = 0; s < params.batch_size_; ++s) {
        int sample_idx = params.batch_offset_ + s;

        // Generate unique ray per sample
        curandState rng;
        curand_init(seed, pixel_idx * params.total_spp_ + sample_idx, 0, &rng);

        Ray ray = generate_camera_ray(idx.x, idx.y, &rng);
        float3 radiance = path_trace(ray, &rng);
        batch_contribution += radiance;
    }

    // Single non-atomic write per thread
    params.accumulator_[pixel_idx] += batch_contribution / float(params.total_spp_);
}
```

**3. New accumulation kernel:**

```cpp
// device/kernels/accumulate_samples.cu
__global__ void accumulate_samples_kernel(
    float3* __restrict__ accumulator,
    const float4* __restrict__ batch_buffer,
    size_t batch_size, size_t image_size
) {
    const size_t pixel_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (pixel_idx >= image_size) return;

    float3 sum = accumulator[pixel_idx];
    for (size_t s = 0; s < batch_size; ++s) {
        const auto sample = batch_buffer[s * image_size + pixel_idx];
        sum += make_float3(sample.x, sample.y, sample.z);
    }
    accumulator[pixel_idx] = sum;
}
```

**4. Host render loop:**

```cpp
void Renderer::render() {
    initStaticParams();  // Once at start

    constexpr size_t BATCH_SIZE = 64;  // Tune based on available VRAM
    const size_t num_batches = (spp + BATCH_SIZE - 1) / BATCH_SIZE;

    for (size_t batch = 0; batch < num_batches; ++batch) {
        const size_t batch_start = batch * BATCH_SIZE;
        const size_t samples_in_batch = min(BATCH_SIZE, spp - batch_start);

        updateDynamicParams(batch_start, samples_in_batch);

        pipeline_.launch(stream, params_ptr, params_size, sbt,
                        width, height, 1);  // Note: Z=1, batch handled internally

        spdlog::info("Batch {}/{}: {} samples rendered",
                    batch + 1, num_batches, batch_start + samples_in_batch);
    }

    image_.saveFinal(output_path);
}
```

### Additional Benefits

| Benefit | Description |
|---------|-------------|
| **Progressive rendering** | See image converge in real-time |
| **Fault tolerance** | Crash at batch N → keep batches 1 to N-1 |
| **Early termination** | Stop when converged (variance threshold) |
| **Debugging** | See artifacts after first batch (~2-5 seconds) |
| **Checkpointing** | Save intermediate results for long renders |

### Optimal Batch Size

```
RTX 2080 (8 GB available ~6.5 GB after env map, GAS, IAS):
  1080p: 6.5 GB / (1920×1080×16) ≈ 200 samples/batch
  4K:    6.5 GB / (3840×2160×16) ≈ 50 samples/batch

Recommended:
  Interactive/debugging: 16-32 samples/batch (fast updates)
  Production:            64-128 samples/batch (balanced)
```

---

## 4. Testing Framework

### Test Suite Architecture

**Single test runner binary** with programmatic scene definitions:

```
test/
├── scenes/
│   ├── geometric_validation.h    # Test scene definitions
│   └── geometric_validation.cpp  # Test scene implementations
├── test_runner.cpp               # Main entry point
└── CMakeLists.txt                # Test build configuration
```

### Required Renderer Refactoring

**Current:** Primitives hardcoded in `initPrimsAndGAS()` with `#define NUM_PRIMITIVES 1`

**Required:** Constructor accepts optional primitives vector:

```cpp
class Renderer {
public:
    explicit Renderer(const app::Config& config,
                     std::vector<params::Primitive> primitives = {});
private:
    std::vector<params::Primitive> scene_primitives_;
    size_t num_primitives_;
};
```

### Test Scenes

#### Correctness Tests

| Test | Description | Expected Result |
|------|-------------|-----------------|
| `coincident_surfaces` | Two Gaussians at exact same position (red + blue) | Purple, matching single purple Gaussian |
| `partial_overlap` | Two Gaussians partially intersecting | Three regions: red, purple (overlap), blue |
| `total_overlap` | Small Gaussian inside larger one | Blue core surrounded by red halo |
| `depth_ordering` | Three Gaussians at different depths along ray | Correct depth-blended colors |
| `camera_inside` | Camera positioned inside large Gaussian | Red fog everywhere, env map visible through |
| `non_overlapping` | Three separate Gaussians, no intersection | Three distinct colored spheres |

#### Transform Tests

| Test | Description | Validates |
|------|-------------|-----------|
| `transform_scale` | Small, medium, large, anisotropic | Scale transform correctness |
| `transform_rotation` | Same ellipsoid at 0°, 45°, 90° rotations | Quaternion rotation correctness |
| `transform_translation` | Same Gaussian at different positions | Translation correctness |

#### Stress Tests

| Test | Description | Validates |
|------|-------------|-----------|
| `many_gaussians` | 100 Gaussians in 10×10 grid | Hit buffer capacity, sorting |
| `nested_structure` | Three concentric shells | Complex overlap handling |
| `tangent_rays` | Camera at edge of Gaussian | Edge case handling |

### Test Runner CLI

```powershell
# Run single test
.\test_runner.exe --scene=coincident_surfaces --spp=64 --output=test.exr

# Run all tests
.\test_runner.exe --all --spp=128 --output-dir=test_results

# List available tests
.\test_runner.exe --list
```

### Verification Checklist

- [ ] `thesis.exe` still works with default scene (backward compatibility)
- [ ] Each test runs without crashes
- [ ] Output images match expected descriptions
- [ ] Camera-inside detection works correctly
- [ ] 100-Gaussian stress test doesn't overflow hit buffer

---

## 5. Profiling Strategy

### Tools

| Tool | Purpose | When to Use |
|------|---------|-------------|
| **NSight Systems** (`nsys`) | Timeline view, kernel launch overhead | First pass: understand overall flow |
| **NSight Compute** (`ncu`) | Detailed kernel analysis | Deep dive: specific kernel optimization |
| **CUDA occupancy calculator** | Thread/warp utilization | Stack pressure analysis |

### Commands

```bash
# Timeline profiling
nsys profile --stats=true --output=profile_report ./thesis.exe

# Kernel-level analysis
ncu --set full --output=kernel_report ./thesis.exe

# Quick occupancy check
ncu --metrics sm__warps_active.avg.pct_of_peak_sustained_active ./thesis.exe
```

### Key Metrics

| Metric | Target | Indicates |
|--------|--------|-----------|
| `sm__warps_active.avg` | >50% | Good occupancy |
| Time in BVH traversal | <50% | BVH not bottleneck |
| Time in optical depth | <30% | Integration not bottleneck |
| Time in sorting | <10% | Sorting not bottleneck |

### Profiling Protocol

1. **Baseline render** at target resolution/spp (after batching implemented)
2. **Identify hotspots** using timeline view
3. **Drill down** into expensive kernels with NSight Compute
4. **Apply targeted optimizations** based on actual data
5. **Re-profile** to verify improvement

### Expected Bottleneck Distribution

Based on similar renderers, expect:

```
BVH Traversal (OptiX trace):     40-60%
Optical Depth Integration:        20-30%
Sorting:                          5-15%
Random Number Generation:         5-10%
Memory Operations:                5-10%
```

---

## 6. Optimization Roadmap

### Priority 0: Memory Architecture (MANDATORY)

| Item | Status | Impact | Effort |
|------|--------|--------|--------|
| Batched online averaging | TODO | Enables production rendering | 1-2 days |
| Fix `uploadParams()` redundancy | TODO | 100-500 μs/frame saved | 1 hour |

### Priority 1: Low-Hanging Fruit

| Item | Status | Impact | Effort |
|------|--------|--------|--------|
| Reduce MAX_CAPACITY (2000→256) | TODO | 5-10× occupancy improvement | 30 min |
| Loop unrolling (`#pragma unroll`) | TODO | 1.2-1.5× on tight loops | 30 min |
| Stream-specific sync (vs device sync) | TODO | Microseconds (correctness) | 5 min |

### Priority 2: Profile-Driven

| Item | Condition | Impact | Effort |
|------|-----------|--------|--------|
| Warp-level optical depth | If integration >30% | 1.5-2× | Medium |
| Early termination (batched sort) | If scattering early | 3-10× | Medium |
| SIMD vectorization (float4) | If integration bottleneck | 1.5-3× | Medium |
| Global memory pool | If stack pressure remains | Better occupancy | Medium |

### Priority 3: Major Refactors (Only If Needed)

| Item | Condition | Impact | Effort |
|------|-----------|--------|--------|
| Warp-cooperative model | If BVH >50% of time | 5-30× | Very High |
| Warp-cooperative sorting | Requires model change | 2-4× | Very High |
| Warp-cooperative hit collection | Requires model change | 5-20× | Very High |

### NOT Recommended

| Item | Reason |
|------|--------|
| `optixReorder` | Requires hit object API, incompatible with current architecture |
| Per-sample launches | Excessive overhead, overkill for progressiveness |
| Double-buffered multi-stream | <0.1% overlap opportunity, not worth complexity |

### CPU Optimizations

| Item | Priority | Impact | Effort |
|------|----------|--------|--------|
| Move camera-inside detection to init | High | 100-500 μs/frame | 30 min |
| Chunked PLY loading | Low | 1.5-2× for N>10k | 10 min |
| Parallel EXR channel deinterleaving | Low | 3-6× on export | 20 min |
| `std::execution::par` for init loops | Medium | 4-8× for large N | 30 min |

---

## 7. Mitsuba Validation

### Objective

Compare this renderer against a **Mitsuba fork** implementing the same volumetric primitives to validate:

1. **Correctness:** Both renderers produce same results (within statistical variance)
2. **Performance:** Measure relative speed
3. **Quality:** Convergence rate comparison

### Quality Metrics

| Metric | Description | Target |
|--------|-------------|--------|
| **PSNR** | Peak Signal-to-Noise Ratio | >40 dB (visually identical) |
| **SSIM** | Structural Similarity Index | >0.99 |
| **MSE** | Mean Squared Error | <1e-4 |
| **Visual** | Side-by-side comparison | No perceptible difference |

### Comparison Protocol

1. **Scene setup:** Identical primitives, camera, environment map
2. **Render conditions:**
   - Same resolution (e.g., 1920×1080)
   - Same sample count (e.g., 1024 spp)
   - Same random seed (if possible)
3. **Output:** Both renderers produce EXR files
4. **Analysis:**
   - Compute PSNR/SSIM/MSE
   - Generate difference image
   - Visual inspection

### Expected Differences

Minor numerical differences expected due to:
- Different random number generators
- Different floating-point evaluation order
- Different intersection epsilon handling

**Acceptable threshold:** PSNR >40 dB, SSIM >0.99

### Convergence Comparison

Render at increasing sample counts and plot:
- Error vs. sample count
- Render time vs. sample count
- Error vs. render time (efficiency curve)

---

## 8. Future Enhancements

### Next Event Estimation (NEE)

**Impact:** 10-100× faster convergence for scenes with emitters

**Current:** Relies on random phase function sampling to hit environment emitter (high variance)

**With NEE:**
1. At scattering event, directly sample direction toward emitter
2. Compute transmittance along shadow ray
3. Use MIS to combine with indirect sampling

**Implementation requirements:**
- Environment map importance sampling (sample bright regions)
- Phase function PDF evaluation
- MIS weight computation

**Effort:** Medium-High (200-300 lines)

### Anisotropic Phase Functions

**Impact:** More physically accurate scattering

**Current:** Isotropic phase function (uniform sphere sampling)

**With Henyey-Greenstein:**
```cpp
float phase_hg(float cos_theta, float g) {
    // g ∈ [-1, 1]: anisotropy parameter
    // g = 0: isotropic, g > 0: forward, g < 0: backward
    float denom = 1.0f + g*g - 2.0f*g*cos_theta;
    return (1.0f / (4.0f * PI)) * (1.0f - g*g) / (denom * sqrtf(denom));
}
```

**Use cases:**
- Forward scattering (g > 0): fog, smoke, clouds
- Backward scattering (g < 0): certain dust particles

**Effort:** Medium (250-300 lines)

### PLY Asset Loading

**Current:** Primitives hardcoded in renderer

**Required:**
1. Parse PLY header for property layout
2. Load property arrays (position, rotation, scale, albedo, sigma_t)
3. Convert to `Primitive` objects

**Already implemented:** `utils/io.cpp` has `loadPrimitives()` function (needs integration)

### Additional Features

| Feature | Impact | Effort |
|---------|--------|--------|
| Adaptive sampling | Stop when converged | Medium |
| Spectral rendering | Wavelength-dependent scattering | High |
| Nested dielectrics | Proper refractive index handling | High |
| Motion blur | Temporal sampling | Medium |
| Depth of field | Lens sampling | Low |

---

## 9. Implementation Schedule

### Week 1: Productization

**Days 1-2: Batched Rendering (CRITICAL)** ✅ COMPLETE
- [x] Add accumulator buffer to Image class
- [x] Reduce sample buffer to batch-sized
- [x] Add batch_offset to LaunchParams
- [x] Implement accumulation kernel
- [x] Refactor render() loop for batching
- [x] Test: 1080p × 1024 spp within memory limits
- [x] Validate: Near-perfect linear scaling confirmed

**Day 3: Host Optimizations** 🔄 IN PROGRESS
- [ ] Move camera-inside detection to constructor
- [ ] Implement `initStaticParams()` / `updateDynamicParams()`
- [x] Change `cudaDeviceSynchronize` to stream-specific sync

**Days 4-5: Renderer Refactoring for Testing**
- [ ] Modify Renderer constructor to accept primitives
- [ ] Extract `createDefaultScene()` method
- [ ] Remove `#define NUM_PRIMITIVES`
- [ ] Verify backward compatibility with `thesis.exe`

### Week 2: Testing & Validation

**Days 1-3: Test Suite Implementation**
- [ ] Create test scene infrastructure
- [ ] Implement 6 correctness tests
- [ ] Implement 3 transform tests
- [ ] Implement 3 stress tests
- [ ] Create test runner with CLI
- [ ] Update CMake for test executable

**Days 4-5: Test Execution & Profiling**
- [ ] Run all tests, verify visual results
- [ ] Profile baseline performance with NSight Systems
- [ ] Identify actual bottlenecks
- [ ] Document findings

### Week 3: Optimization & Mitsuba Comparison

**Days 1-2: Profile-Driven Optimization**
- [ ] Apply Priority 1 optimizations (MAX_CAPACITY, unrolling)
- [ ] If needed: warp-level optical depth
- [ ] Re-profile and measure gains

**Days 3-5: Mitsuba Validation**
- [ ] Set up identical test scenes
- [ ] Render with both renderers
- [ ] Compute quality metrics (PSNR, SSIM)
- [ ] Document comparison results

### Deliverables

| Week | Deliverable |
|------|-------------|
| 1 | Production-capable renderer (4K @ 1024+ spp) |
| 2 | Validated geometric correctness + profiling data |
| 3 | Performance-optimized renderer + Mitsuba comparison |

---

## Appendix A: Known Issues & Limitations

### Architectural

| Issue | Impact | Mitigation |
|-------|--------|------------|
| Large hit buffer (24 KB stack) | Low occupancy | Reduce to 256-512 elements |
| One thread = one ray model | No warp cooperation | Accept for now, refactor if needed |
| No nested dielectrics | Overlapping volumes add densities | Acceptable for Gaussian splats |

### Fundamental

| Limitation | Description |
|------------|-------------|
| Isotropic phase only | No directional scattering (implement HG later) |
| Single wavelength | No spectral rendering |
| Fixed optical thickness | All Gaussians use same σ parameter |
| No surface geometry | Only analytic density fields |

### Performance

| Issue | Description |
|-------|-------------|
| No spatial acceleration | Relies on OptiX IAS/GAS |
| Sequential per-ray processing | No warp-level cooperation |
| Warp divergence at high bounces | RR causes idle threads |

---

## Appendix B: Reference Commands

### Build & Run

**IMPORTANT:** Do NOT attempt to build using `ninja` or `Bash` tools. Building requires Visual Studio Developer Command Prompt with properly configured environment variables.

```powershell
# Build (in VS Developer Command Prompt)
ninja -C build

# Run production render
.\build\bin\Release\thesis.exe --width=1920 --height=1080 --spp=1024 --output=render.exr

# Run with debug output
.\build\bin\Release\thesis.exe --debug=true
```

### Profiling

```bash
# Timeline analysis
nsys profile --stats=true --output=profile .\thesis.exe

# Kernel analysis
ncu --set full --output=kernel_analysis .\thesis.exe

# Occupancy check
ncu --metrics sm__warps_active.avg.pct_of_peak_sustained_active .\thesis.exe
```

### Testing

```powershell
# Run specific test
.\build\bin\Release\test_runner.exe --scene=coincident_surfaces --spp=64

# Run all tests
.\build\bin\Release\test_runner.exe --all --spp=128 --output-dir=test_results

# List tests
.\build\bin\Release\test_runner.exe --list
```

---

## Appendix C: File Quick Reference

### Critical Files to Modify (Batched Rendering)

| File | Change |
|------|--------|
| `include/thesis/host/params/image.h` | Add accumulator buffer, batch handling |
| `device/params/image.h` | Add batch_offset, total_spp to device struct |
| `device/entry/raygen.cuh` | Loop over batch samples, accumulate |
| `src/thesis/host/app/renderer.cpp` | Batch render loop, initStaticParams() |

### Critical Files to Modify (Testing)

| File | Change |
|------|--------|
| `include/thesis/host/app/renderer.h` | Constructor with primitives parameter |
| `src/thesis/host/app/renderer.cpp` | num_primitives_ member, createDefaultScene() |
| `test/scenes/geometric_validation.cpp` | Test scene implementations |
| `test/test_runner.cpp` | Test harness |

### Core Algorithm Files

| File | Purpose |
|------|---------|
| `device/entry/raygen.cuh` | Path tracing loop |
| `device/core/sampling.cuh` | Scattering, optical depth |
| `device/core/sorting.cuh` | Hit buffer sorting |
| `device/entry/anyhit.cuh` | Hit collection |

---

## Appendix D: External References

- **OptiX Programming Guide:** https://raytracing-docs.nvidia.com/optix7/guide/index.html
- **CUDA Programming Guide:** https://docs.nvidia.com/cuda/cuda-c-programming-guide/
- **CUDA Best Practices:** https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/
- **Physically Based Rendering:** https://www.pbr-book.org/
- **Henyey-Greenstein Phase Function:** Henyey & Greenstein (1941)

---

## Appendix E: Detailed Documentation

For in-depth analysis of specific topics, see the `plans/` directory:
- `OPTIMIZATION_TODO.md` - Detailed GPU optimization strategies
- `CPU_OPTIMIZATION.md` - Host-side optimization analysis
- `CPU_PARALLELISM_ANALYSIS.md` - std::execution::par opportunities
- `PARALLELISM_RESTRUCTURING.md` - Execution model analysis
- `TESTING.md` - Full test scene implementations
- `CUDA_STREAMS_ANALYSIS.md` - Stream architecture analysis
