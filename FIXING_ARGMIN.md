# Fixing the Argmin Implementation: Hybrid Approach for Volumetric Rendering

## Problem Statement

The argmin optimization successfully eliminates sorting when scattering occurs, but breaks down for the escape case (when rays exit all media without scattering). The current implementation incorrectly handles optical depth computation, causing Gaussians to either appear solid or invisible.

## Root Cause Analysis

### The Fundamental Issue

The argmin approach computes scatter points directly using inverse CDF sampling, but when no scattering occurs (all `inv_cdf()` returns > t_exit for all primitives), we need to compute optical depth through all traversed media. The problem is:

1. **Current approach**: `density_integral(ray, 0.0f)` integrates from t=0 to t=∞
2. **What we need**: Bounded integrals [t_entry, t_exit] for each primitive

### Why Current Attempts Failed

| Attempt | Result | Problem |
|---------|--------|---------|
| Include all primitives with [0,∞] | Solid Gaussians | Integrating outside primitive bounds |
| Clear active_prims (empty set) | Invisible Gaussians | Zero optical depth |
| Include only starting primitives | Partial visibility | Missing entered primitives |

The issue isn't WHICH primitives to include, but HOW to integrate them with proper bounds.

## Mathematical Background

### Pure Absorption (No Scattering)

The reference algorithm for pure absorption:
```
For each segment [t_i, t_{i+1}]:
  - Determine active primitives during segment
  - Integrate optical depth: τ += ∑ density_integral(ray, t_i, t_{i+1})
Final transmission: exp(-τ)
```

### With Scattering (Monte Carlo)

The Monte Carlo volume rendering equation:
```
1. Sample scatter distance using inverse CDF
2. If scatter within medium:
   - Evaluate albedo at scatter point
   - Sample new direction
   - Continue recursively
3. If no scatter (escape):
   - Compute total optical depth
   - Apply transmission exp(-τ)
```

### The Argmin Optimization

Based on Analog Decomposition Tracking theorem:
- **Benefit**: Avoid sorting by taking minimum of inverse CDFs
- **Limitation**: Only optimizes scatter point selection, not escape case

## Proposed Solution: Hybrid Approach

### Core Insight

We need TWO different code paths:
1. **Scattering path** (argmin): When chi samples a scatter event
2. **Escape path** (segment-by-segment): When no scattering occurs

### Algorithm Structure

```cpp
bool sample_scattering_event(...) {
    // Sample chi for inverse CDF
    float chi = random_uniform();

    // Try argmin approach for scatter point
    float t_scatter_min = find_min_scatter(ray, chi);

    if (t_scatter_min < INF) {
        // FAST PATH: Scattering occurred
        // Use argmin result directly
        rebuild_active_prims_at_scatter(t_scatter_min);
        return true;
    } else {
        // FALLBACK PATH: No scattering (escape)
        // Need segment-by-segment integration
        compute_escape_optical_depth(ray, active_prims);
        return false;
    }
}
```

### Detailed Escape Case Handler

```cpp
void compute_escape_optical_depth(ray, active_prims) {
    // Collect ALL hit events (entries and exits)
    struct Event {
        float t;
        uint prim_idx;
        bool is_exit;
    };

    vector<Event> events;

    // Add exits for primitives we start inside
    for (prim in active_prims) {
        t_exit = compute_exit(ray, prim);
        events.push_back({t_exit, prim.idx, true});
    }

    // Add entry-exit pairs for primitives we hit
    for (hit in hit_buffer) {
        events.push_back({hit.t_entry, hit.prim_idx, false});
        t_exit = compute_exit(ray, hit.t_entry, prim);
        events.push_back({t_exit, hit.prim_idx, true});
    }

    // Sort events by t-value
    sort(events);

    // Process segments
    float t_prev = 0.0;
    set<uint> current_active;

    // Initialize with primitives we start inside
    current_active = active_prims;

    for (event in events) {
        if (t_prev < event.t) {
            // Integrate current active set over [t_prev, event.t]
            for (prim_idx in current_active) {
                tau += density_integral(ray, prim_idx, t_prev, event.t);
            }
        }

        // Update active set
        if (event.is_exit) {
            current_active.erase(event.prim_idx);
        } else {
            current_active.insert(event.prim_idx);
        }

        t_prev = event.t;
    }

    // Final segment to infinity (should be empty set)
    // No integration needed as all primitives have finite extent

    return tau;
}
```

## Implementation Plan

### Phase 1: Minimal Fix (Quick)
**Goal**: Get correct rendering working again

1. When no scattering occurs, collect all entry/exit events
2. Sort them (yes, we need sorting for escape case)
3. Integrate segment-by-segment with proper bounds
4. Return optical depth for transmission calculation

**Estimated effort**: 2-3 hours

### Phase 2: Optimize Escape Path (Optional)
**Goal**: Improve performance of escape case

1. Use a small fixed-size array for events (avoid dynamic allocation)
2. Implement insertion sort for small N (faster than bitonic for escape events)
3. Consider early termination when optical depth exceeds threshold

**Estimated effort**: 1-2 hours

### Phase 3: Validation
**Goal**: Ensure correctness

1. Compare against pure absorption reference
2. Test edge cases:
   - Ray starts inside multiple primitives
   - Ray grazes primitive boundaries
   - High optical depth (near-opaque media)
   - Low optical depth (near-transparent media)

**Estimated effort**: 1-2 hours

## Key Design Decisions

### Why Not Always Use Segment-by-Segment?

- **Performance**: Argmin is faster when scattering occurs (common case)
- **Complexity**: Argmin code is simpler for scatter point selection
- **Cache**: Argmin has better memory access patterns

### Why Accept Sorting for Escape Case?

- **Correctness**: Required for proper optical depth computation
- **Frequency**: Escape is less common in dense media
- **Bounded cost**: Limited by number of primitives along ray

### Memory Considerations

For escape case, we need:
- Entry/exit events: 2N entries for N primitives
- Event structure: 8 bytes (float t + uint idx + bool flag)
- Total: ~16N bytes temporary storage

For MAX_PRIMITIVES=1024: ~16KB additional stack usage (acceptable)

## Alternative Approaches Considered

### 1. Precompute All Intersections
**Idea**: Always compute entry/exit for all primitives upfront
**Rejected because**: Wasteful when scattering occurs early

### 2. Approximate Optical Depth
**Idea**: Use heuristic for escape case (e.g., sum of densities × average path length)
**Rejected because**: Introduces bias, visible artifacts

### 3. Pure Argmin Without Escape
**Idea**: Force scattering by clamping chi to ensure some primitive scatters
**Rejected because**: Biases the Monte Carlo estimator

## Testing Strategy

### Correctness Tests

1. **Single Gaussian**: Should show proper falloff, not solid
2. **Multiple Gaussians**: Correct overlap and occlusion
3. **Camera inside**: Proper fog effect
4. **High SPP convergence**: Should match reference

### Performance Tests

1. Measure escape case frequency in typical scenes
2. Profile segment-by-segment integration cost
3. Compare overall performance vs. sorted approach

### Edge Cases

1. Ray starts inside primitive with inv_cdf returning negative
2. Numerical precision at primitive boundaries
3. Coincident surfaces (multiple primitives at same location)

## Conclusion

The argmin optimization is valuable but incomplete. We need a hybrid approach:
- **Fast path** (argmin) for scattering: O(N) without sorting
- **Fallback path** (segment-by-segment) for escape: O(N log N) with sorting

This preserves the optimization for the common case while maintaining correctness for all cases. The additional complexity is justified by the performance gains in typical volumetric rendering scenarios where most rays scatter.

## References

1. SDTracking Paper: Analog Decomposition Tracking theorem (Section 4.1)
2. PBRT Book: Volume Rendering chapter
3. Mitsuba: Heterogeneous media implementation

## Status

- [ ] Implement Phase 1 (Minimal Fix)
- [ ] Test correctness
- [ ] Profile performance impact
- [ ] Consider Phase 2 optimizations
- [ ] Document final approach in CLAUDE.md