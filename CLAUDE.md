# Gaussian Volumetric Path Tracer

**Project:** OptiX + CUDA Physically Based Volumetric Renderer
**Target:** Production-quality single-frame renderer for Gaussian volumetric primitives
**Validation:** Comparison against Mitsuba reference implementation
**Status:** ADT-based scattering (argmin approach) working — performance optimization phase
**Origin:** CUDA/C++ rewrite of Jorge Condor's Mitsuba implementation of *Don't Splat Your Gaussians* (DSYG)
**Paper:** `papers/DSYG.pdf`

## Development Guidelines

### Git Workflow

**Branch Strategy:**
- `main` is the single source of truth - always stable and working
- One feature branch per feature/change
- Branch naming: `feature/descriptive-name` (e.g., `feature/batched-rendering`)
- After merge, delete the feature branch immediately

**Commit Process:**
1. Make changes on feature branch
2. Stage relevant files only (exclude unrelated changes)
3. Verify: correctness, code quality
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
- Use `float4` for image buffers (128-bit aligned memory access)
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

---

## Executive Summary

### What This Project Is

A **physically based volumetric path tracer** rendering participating media represented by **Gaussian ellipsoids**. Each primitive is a 3D Gaussian density field. OptiX hardware-accelerated BVH traversal uses unit sphere primitives with instance transforms for acceleration, while optical depth integration is computed analytically using error functions. The renderer uses Monte Carlo sampling for scattering events.

### Reference Implementations

- **Mitsuba reference:** `~/volumetric_primitives` — Jorge Condor's original DSYG implementation with segment-by-segment path tracing + Newton/bisection solver for scatter distance
- **Stochastic Splats:** `~/stochasticsplats` — GPU rasterization framework for sorting-free Gaussian splatting (referenced by Jorge as related work, but it's a rasterizer, not a volumetric path tracer)
- **SDTracking paper:** `papers/SDTracking.pdf` — Describes Analog Decomposition Tracking (ADT) in §4.1 (Theorem 1: min of independent free-flight samples = combined free-flight sample)
- **Stochastic Splats paper:** `papers/stoch-splats.pdf`

### Algorithmic History

**Before commit `1c8b578`:** Segment-by-segment path tracing matching Mitsuba — sorting hits, marching through segments, bisection solver for scatter distance. `inv_cdf` existed in `primitive.h` but was unused. Renders matched reference.

**After commit `1c8b578` (current `feature/test-suite`):** Novel ADT-inspired argmin approach suggested by Jorge. Instead of sorting and marching, each primitive independently samples a scatter distance via `inv_cdf`, and the minimum (argmin) determines where scattering occurs. Eliminates sorting for the scatter case; escape case still uses segment-by-segment integration as fallback. Uses `-log(1-χ)` (free-flight CDF per SDTracking Theorem 1).

### Notes

- The `optical_depth` functions use an unnormalized Gaussian convention (scale factor `(2π)^{3/2}·∏s` larger than Jorge's Mitsuba). The `optical_thickness_` (σ_t) values are calibrated to match this convention.
