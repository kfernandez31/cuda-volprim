# Debug Plan: transform_translation Non-Termination

## Problem Statement

The `transform_translation` test hangs indefinitely even at spp=1, while other tests complete successfully.

## Hypothesis

A device-side printf in `primitive.h:157` may be triggering for many pixels, causing CUDA printf buffer overflow and kernel hang.

```cpp
if (!(t0 <= t1 && isfinite(t1) && t1 > 0.0f)) {
    printf("ERROR: optical_depth assertion would fail! t0=%.6f, t1=%.6f, isfinite(t1)=%d\n",
           t0, t1, isfinite(t1));
    return 0.0f;
}
```

## Test Configuration

**Camera position:** `(0, 0, -1)` looking at `(0, 0, 0)`
**Resolution:** 1920×1080 (reduce to 800×600 for faster testing)
**SPP:** 1 (minimal)

## Minimal Test Cases

### Test 1: `debug_single_at_origin`
**Setup:** Single Gaussian at (0,0,0), scale 0.4
**Expected:** Should work (similar to other working tests)
**Purpose:** Isolate single primitive case

### Test 2: `debug_single_offset`
**Setup:** Single Gaussian at (1,0,0), scale 0.4
**Expected:** Should work (away from camera line-of-sight)
**Purpose:** Verify offset prevents issue

### Test 3: `debug_two_at_origin`
**Setup:** Two Gaussians at (0,0,0), scale 0.4
**Expected:** Should work (coincident_surfaces test passes)
**Purpose:** Test if multiple overlapping primitives matter

### Test 4: `debug_grid_2x2`
**Setup:** 2×2 grid at positions (0,0,0), (1,0,0), (0,1,0), (1,1,0)
**Expected:** Unknown
**Purpose:** Test if grid pattern causes issue

## Testing Procedure

```bash
# Build with changes
ninja -C build

# Test each case with timeout
timeout 30s ./build/bin/Release/test_runner.exe --scene=debug_single_at_origin --spp=1 --width=800 --height=600
timeout 30s ./build/bin/Release/test_runner.exe --scene=debug_single_offset --spp=1 --width=800 --height=600
timeout 30s ./build/bin/Release/test_runner.exe --scene=debug_two_at_origin --spp=1 --width=800 --height=600
timeout 30s ./build/bin/Release/test_runner.exe --scene=debug_grid_2x2 --spp=1 --width=800 --height=600
```

## Expected Outcomes

| Test | Terminates? | Inference |
|------|-------------|-----------|
| single_at_origin | ✓ | Not a single primitive issue |
| single_at_origin | ✗ | **Minimal case found!** |
| single_offset | ✓ | Position matters |
| single_offset | ✗ | Not position-specific |
| two_at_origin | ✗ | Overlapping primitives trigger it |
| grid_2x2 | ✗ | Grid pattern triggers it |

## Next Steps After Testing

1. **If single_at_origin hangs:** Capture with debug=true for one pixel
2. **If all pass:** Add more primitives to find threshold
3. **If pattern emerges:** Focus on that specific configuration

## Diagnostic Enhancement (If Needed)

Replace printf with atomic counter to avoid buffer overflow:

```cpp
// Add to launch_params
__device__ uint* error_counter;

// In primitive.h:156
if (!(t0 <= t1 && isfinite(t1) && t1 > 0.0f)) {
    atomicAdd(launch_params.error_counter, 1);
    return 0.0f;
}

// After kernel: read counter from host
```

This would definitively show if the condition is being triggered without risk of hang.
