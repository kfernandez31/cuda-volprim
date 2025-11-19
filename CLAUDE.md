# Project Summary

This repository implements a **physically based volumetric path tracer** using **OptiX + CUDA**.
It renders participating media represented by **tessellated icospheres** that act as shells enclosing Gaussian ellipsoids.
OptiX built-in spheres were avoided since they fail to register hits when a ray originates inside them.

Each primitive contributes **two intersections per ray**: entry (`t_in`) and exit (`t_out`).
Between those bounds, the renderer assumes a continuous medium and integrates density and optical depth analytically or via numerical solvers.

---

## Current Status

### What Works ✅
- Single Gaussian renders correctly with proper scattering
- Entry/exit face detection via `OPTIX_HIT_KIND_TRIANGLE_BACK_FACE`
- Analytical optical depth integration using erf functions
- Monte Carlo scattering with exponential sampling (τ = -ln(1-χ))
- Russian roulette path termination
- Environment map lighting
- Ray-starts-inside handling via `initial_active_prims` parameter

### Current Implementation: Anyhit Buffer-Based Collection

**Migration in progress** from incremental trace approach to full-ray collection.

**Problem being solved:**
- OptiX's `optixTrace` returns only ONE closest hit per call
- When multiple Gaussians have coincident surfaces (exact same t-value), only one is detected
- Previous approaches using epsilon-based advancement or bounded searches failed

**New approach (currently being built):**
1. **One `optixTrace` call per ray** - collects ALL hits via anyhit program
2. **Anyhit always ignores** hits (calls `optixIgnoreIntersection()`) to continue traversal
3. **Hits stored in buffer** via payload slots 4-5 (pointer to `HitBuffer`)
4. **Sort by t-value** once after collection
5. **Process incrementally** - early termination still possible during processing

**Key files modified:**
- `device/entry/anyhit.cuh` - Collection mode, stores all hits in buffer
- `device/core/trace.cuh` - New `trace_ch_collect()` function with `OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT`
- `device/core/sampling.cuh` - Refactored `sample_scattering_event()` and `compute_optical_depth_along_ray()`
- `device/core/hit_record.cuh` - New struct for storing hit data
- `device/core/sorting.cuh` - Bubble sort implementation
- `device/core/constants.cuh` - Added `MAX_CAPACITY = 2000` for both buffer and active set
- `include/thesis/host/app/config.h` - Added `anyhit_function_name_`, removed closesthit/intersection
- `include/thesis/host/optix/program_group.h` - Made closesthit optional in `createHitgroup()`
- `src/thesis/host/app/renderer.cpp` - Set `numPayloadValues = 6`, use config for anyhit name

**Current compilation issue:** Being debugged (payload slots configuration)

---

## Key Components

### OptiX Programs
- **`__raygen__rg`** (`device/entry/raygen.cuh`) - Main ray generation, scattering loop, Russian roulette, environment lighting
- **`__anyhit__ah`** (`device/entry/anyhit.cuh`) - Collects all hits into buffer, always ignores to continue traversal
- **`__miss__ms`** (`device/entry/miss.cuh`) - Returns environment color
- **`__closesthit__ch`** (`device/entry/closesthit.cuh`) - DEPRECATED (disabled via ray flags)

### Core CUDA Modules
- **`sampling.cuh`** - Scattering event sampling, optical depth integration, albedo evaluation
  - `sample_scattering_event()` - Collects hits, sorts, processes until scattering
  - `compute_optical_depth_along_ray()` - Computes transmittance along ray
  - `sample_distance_bisection()` - Finds exact scattering position
- **`trace.cuh`** - OptiX ray tracing wrappers
  - `trace_ch_collect()` - Collects all hits via anyhit buffer
- **`launch_params.cuh`** - Camera, primitives, env map, image buffers
- **`hit_record.cuh`** - Struct for intersection data (t, prim_idx, is_exit)
- **`sorting.cuh`** - Bubble sort for hit records by t-value
- **`payload_utils.cuh`** - Pointer packing/unpacking for OptiX payloads

### Core Model
- **Optical depth sampling:** τ = −ln(1 − χ)
- **Transmittance:** T = exp(−τ)
- **Phase function:** Isotropic (1 / 4π)
- **Medium evaluation:** Monte Carlo integration over entry–exit segments
- **Gaussian representation:** Analytic density in "whitened" local space, integrated via erf functions
- **No true volume geometry** - only analytic density fields

---

## Recent Problem History

### Issue 1: Coincident Surfaces ✅ SOLVED (single Gaussian)
**Problem:** Multiple primitives with surfaces at exact same t-value would be missed or incorrectly processed.

**Failed approaches:**
1. Epsilon-based advancement - skipped coincident hits
2. Anyhit filtering with `processed_this_t` set - only found one hit per t-cluster
3. Two-phase trace (bounded/unbounded) - OptiX still returned only one hit at exact t-value
4. Symmetric epsilon bounds `[t_total - eps, t_total + eps]` - caused negative t_min or integration errors

**Root cause:** OptiX's `optixTrace` returns only ONE closest hit, even if multiple primitives have hits at the exact same t. When anyhit rejects a hit, OptiX doesn't automatically report the next one at the same t.

**Solution in progress:** Anyhit buffer-based collection - anyhit sees ALL hits during traversal, so collect everything in one trace call.

### Issue 2: Ray-Starts-Inside ✅ SOLVED
**Problem:** Rays spawned inside primitives (after scattering) only detect exit faces.

**Solution:** Pass `initial_active_prims` to `compute_optical_depth_along_ray()`, preserve `event.active_prims_` between bounces.

### Issue 3: Scene-Wide Color Tinting 🔍 INVESTIGATING
**Problem:** Entire scene is slightly tinted by Gaussian's color, even pixels that shouldn't intersect it.

**Hypothesis:** Incorrect optical depth computation for rays that miss geometry, or incorrect handling of empty `active_prims`.

**Status:** Should be resolved by anyhit buffer approach (cleaner hit detection).

---

## Project Roadmap

### Phase 1: Fix Coincident Surfaces (IN PROGRESS)
- [x] Implement anyhit buffer-based collection
- [x] Remove old incremental trace logic
- [x] Update config to remove closesthit references
- [ ] **Fix compilation errors** (payload count - currently at this step)
- [ ] **Test with two identical Gaussians** at same position
- [ ] **Verify both are detected** and render correctly (red + blue = purple/magenta)
- [ ] **Verify scene tinting is gone**

### Phase 2: Geometry Validation
Test various geometric configurations to ensure correctness:

**Transform Testing:**
- [ ] Different scales (small, large, anisotropic)
- [ ] Different rotations (arbitrary quaternions)
- [ ] Different translations (near/far from camera)
- [ ] Edge cases (very small Gaussians, very large Gaussians)

**Overlap Scenarios:**
- [ ] Gaussian-in-Gaussian (one completely inside another)
- [ ] Gaussian-behind-Gaussian (along ray direction)
- [ ] Partial overlaps (various intersection patterns)
- [ ] Multiple overlaps (3+ Gaussians at same point)
- [ ] Non-overlapping Gaussians (ensure no interference)

**Stress Tests:**
- [ ] Many primitives (100+) along single ray
- [ ] Complex nested structures
- [ ] Edge hits (ray tangent to surface)

### Phase 3: Production Rendering & Validation
**Asset Loading:**
- [ ] Implement `.ply` file loader for point clouds
- [ ] Convert point cloud to Gaussian primitives (fit ellipsoids)
- [ ] Render production asset

**Benchmarking vs Mitsuba:**
- [ ] Implement Mitsuba-based reference renderer (Python)
- [ ] Render same scene with both renderers
- [ ] **Quality metrics:**
  - PSNR (Peak Signal-to-Noise Ratio)
  - SSIM (Structural Similarity Index)
  - MSE (Mean Squared Error)
  - Visual comparison (side-by-side images)
- [ ] **Performance metrics:**
  - Render time per sample
  - Samples per second
  - Memory usage
  - Convergence rate

### Phase 4: Optimization (Profile-Driven)
**Priority 1 (Low-Hanging Fruit):**
- [ ] Add `optixReorder()` for ray coherence (1 line, 1.5-3x win)
- [ ] Reduce `MAX_CAPACITY` to 256-512 (improve occupancy)
- [ ] Adaptive sort (warp shuffle for n<32, bitonic for n<1024, radix for n>1024)
- [ ] Loop unrolling via `#pragma unroll`

**Priority 2 (Profile-Driven):**
- [ ] Early termination with batched sorting
- [ ] SIMD vectorization (if integration is bottleneck)
- [ ] Global memory pool (if stack pressure remains)

**Priority 3 (Major Refactors):**
- [ ] Warp-cooperative hit collection (if BVH traversal dominates)
- [ ] Cooperative warp integration (if integration dominates)

See `OPTIMIZATION_TODO.md` for detailed optimization strategies.

### Phase 5: Thesis Writing
- [ ] Document algorithm and implementation
- [ ] Present benchmark results
- [ ] Analyze performance characteristics
- [ ] Compare with existing solutions
- [ ] Discuss limitations and future work

---

## Known Issues & Limitations

### Current
- **Coincident surface detection** - Being fixed with anyhit buffer approach
- **Scene tinting** - Under investigation, likely related to above
- **Large buffer size** - 2000 elements × 12 bytes = 24 KB stack per thread (occupancy concern)
- **Debug mode broken** - Uses old `trace_ch()` which no longer works with new anyhit

### Fundamental
- **No nested dielectrics** - Overlapping volumes simply add densities
- **Isotropic phase only** - No anisotropic scattering (HG, Mie, etc.)
- **Single wavelength** - No spectral rendering or dispersion
- **Fixed optical thickness** - All Gaussians use same σ parameter

### Performance
- **O(n²) bubble sort** - Needs optimization for production scenes
- **No spatial acceleration** for primitives (relies on OptiX IAS/GAS)
- **Single-threaded per ray** - No warp-level cooperation yet

---

## Architecture Overview

### Host (CPU) Code
**Location:** `src/thesis/host/`, `include/thesis/host/`

- **`app/renderer.cpp`** - Main renderer class, OptiX pipeline setup
- **`app/config.h`** - CLI configuration (resolution, samples, entry points)
- **`optix/pipeline.h`** - OptiX pipeline management
- **`optix/program_group.h`** - Program group creation (raygen, miss, hitgroup)
- **`optix/module.h`** - OptiX module loading
- **`optix/sbt.h`** - Shader Binding Table construction
- **`cuda/context.h`** - CUDA context management
- **`params/primitive.h`** - Host-side primitive parameters (transforms, albedo, etc.)

### Device (GPU) Code
**Location:** `device/`

**Entry points:**
- `entry/raygen.cuh` - Ray generation and path tracing loop
- `entry/anyhit.cuh` - Hit collection into buffer
- `entry/miss.cuh` - Environment lookup
- `entry/closesthit.cuh` - DEPRECATED (unused)

**Core algorithms:**
- `core/sampling.cuh` - Scattering, optical depth, albedo evaluation
- `core/trace.cuh` - OptiX trace wrappers
- `core/sorting.cuh` - Hit buffer sorting
- `core/hit_record.cuh` - Intersection data structure
- `core/random.cuh` - CURAND wrappers for sampling
- `core/constants.cuh` - Device constants (MAX_CAPACITY, eps, etc.)
- `core/payload_utils.cuh` - Pointer packing for OptiX payloads
- `core/debug.cuh` - Debug thread detection helper

**Data structures:**
- `utils/vector.h` - StaticVector and DynamicVector
- `utils/set.h` - Set with linear/binary search policies
- `params/primitive.h` - Device-side Gaussian primitive with optical depth integration

---

## Build & Run

**IMPORTANT:** Do NOT attempt to build the project using the `ninja` or `Bash` tools. Building requires a special Visual Studio Developer Command Prompt environment with properly configured compiler paths and environment variables. The user will handle compilation themselves.

**Build:**
```powershell
ninja -C build
```
(Must be run in Visual Studio developer environment)

**Run:**
```powershell
.\build\bin\Release\thesis.exe --debug=true
```

**Key flags:**
- `--debug=true` - Enable debug logging for center pixel
- `--width=1000` - Image width
- `--height=750` - Image height
- `--samples_per_pixel=1` - Samples per pixel
- `--output=output.exr` - Output path
- `--anyhit=__anyhit__ah` - Anyhit function name

---

## Next Immediate Steps

1. **Fix current compilation error** - Payload count mismatch (set to 6 in renderer.cpp)
2. **Build and test** with two identical Gaussians at same position
3. **Verify both primitives detected** in debug output
4. **Verify scene tinting resolved**
5. **Move to Phase 2** (geometry validation) once coincident surfaces work correctly

---

## References

- **OptiX Programming Guide:** https://raytracing-docs.nvidia.com/optix7/guide/index.html
- **CUDA Programming Guide:** https://docs.nvidia.com/cuda/cuda-c-programming-guide/
- **Physically Based Rendering (PBR Book):** https://www.pbr-book.org/
- **Optimization strategies:** See `OPTIMIZATION_TODO.md`
