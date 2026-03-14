# Gaussian Volumetric Path Tracer

**Project:** OptiX + CUDA Physically Based Volumetric Renderer
**Target:** Production-quality single-frame renderer for Gaussian volumetric primitives
**Validation:** Comparison against Mitsuba reference implementation
**Status:** Production-ready renderer complete, entering validation phase with Jorge's asset

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
