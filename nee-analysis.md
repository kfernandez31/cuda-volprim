# NEE Implementation Analysis

## What Was Done

Added Next Event Estimation (NEE) to the volumetric path tracer. At each scatter event,
instead of relying on the path randomly escaping toward a bright environment direction, we
explicitly fire a shadow ray, compute transmittance along it, and add the direct lighting
contribution:

```
radiance += throughput * albedo * env(light_dir) * transmittance
```

## Bug 1: Double-Counting (Fixed)

### Problem

The original NEE implementation added both:
1. NEE contribution at each scatter (shadow ray → environment)
2. Escape contribution when the path eventually leaves the medium

Both estimate the same physical quantity — light traveling from a scatter point out to
the environment. For a single-scatter path:

```
NEE:    throughput * albedo * env(light_dir) * T          ← camera → scatter → env
Escape: throughput * albedo * exp(-τ) * env(escape_dir)   ← same path type, again
```

This caused roughly 2× over-estimation in sparse-medium regions (where T ≈ 1), producing
large bright blobs colored by each Gaussian's albedo. Dense regions were unaffected
(T ≈ 0 dampens the NEE term).

The non-NEE scatter estimate `throughput * albedo * env * PHASE_VALUE` was negligible
(PHASE_VALUE ≈ 0.08), so the pre-NEE renders were dominated by the escape term alone and
didn't show this artifact.

### Fix

Only add the escape contribution for transmitted paths (bounce == 0), i.e. rays that
passed through the medium without scattering. For those paths, no NEE ever fired, so
the escape is the only contribution:

```cpp
if (!consts::ENABLE_NEE || bounce == 0) {
    radiance += (throughput * math::exp(-tau)) * miss.color();
}
```

## Bug 2: Remaining Brightness Difference (Unresolved)

### Observation

After the double-counting fix, renders 71–73 (fixed NEE) were compared against renders
68–70 (pre-NEE) at higher SPP. The difference persisted and was systematic, not noise:

| Gaussian | Channel | Pre-NEE | Fixed NEE | Ratio |
|----------|---------|---------|-----------|-------|
| Red      | R/G/B   | ~same   | ~same     | ~1.00 |
| Green    | G       | 0.113   | 0.208     | 1.84× |
| Green    | B       | 0.048   | 0.110     | 2.29× |
| Blue     | B       | 0.762   | 1.029     | 1.35× |

The difference correlated with Gaussian density (red = sparse, green/blue = denser).

### Root Cause: Estimators Accumulate Different Scatter Orders Per Path

For a path that scatters N times:

| Estimator   | Contribution per path |
|-------------|-----------------------|
| Pre-NEE     | `albedo^N * T * env` — one term (the Nth scatter order) |
| Fixed NEE   | `Σ albedo^k * T * env` for k=1..N — all orders 1 through N |

Fixed NEE accumulates direct lighting at every bounce. A single long path contributes
estimates for scatter orders 1 through N simultaneously. Pre-NEE only contributes the
Nth-order term — lower orders come from separately sampled shorter paths.

For high-albedo media (many bounces), this is dramatic. At albedo=0.9, N=10:
- Pre-NEE single path: `0.9^10 ≈ 0.35`
- Fixed NEE single path: `0.9 + 0.81 + ... ≈ 5.8`

Pre-NEE recovers the full sum over many path samples of varying lengths. Fixed NEE
gets all orders from one path — it's more sample-efficient but converges to the same
mean only if sampling is correct.

### Why They Should Agree (Theory)

Both are theoretically unbiased. For NEE at bounce k, the contribution is:

```
E[NEE at k] = P(reach k) * throughput_k * albedo_k * T * env
```

The RR weight (1/p_survive in throughput) cancels the RR survival probability in
P(reach k), giving E[NEE at k] = L_{k+1} (the (k+1)-th Neumann series term).
Summing over all k gives the full Neumann series = correct radiance.

Pre-NEE (pure path tracer) is also unbiased: the escape at bounce N, weighted by
RR compensation in throughput, gives E[escape_N] = L_N.

### Why They Don't Agree In Practice

Three candidate explanations, not yet resolved:

**1. Convergence rate difference (most likely)**
Pre-NEE needs many short paths to sample low-order scattering terms. At finite SPP,
if short paths are under-represented (e.g. dense medium makes single-scatter rare),
pre-NEE under-estimates. Fixed NEE gets all orders from each long path and may have
already converged while pre-NEE hasn't.

**2. Bug in compute_transmittance_to_env**
Active primitives (those containing the scatter point) may be double-counted in the
shadow ray transmittance computation. `collect_hits` traces from inside those
primitives and picks up their exit hits, which are then labeled as entry events in
the hit buffer. After sorting (exits before entries at same t), the primitive is
removed at its exit point then immediately re-added with no future exit, causing it
to contribute extra optical depth for all t beyond that point.

Direction of error: makes transmittance *lower* (shadow too dark), which would make
NEE *darker* than correct — opposite of what we observe. So this bug may not be the
primary cause of the brightness difference, but it is a correctness issue.

**3. Pre-NEE is the ground truth and fixed NEE is still biased**
If the pre-NEE matches Mitsuba (the reference implementation), then fixed NEE being
brighter would indicate a remaining bias. Not verified.

## What Is Needed to Resolve

A Mitsuba reference render of the same scene is required to establish ground truth.
Compare both pre-NEE and fixed NEE against it to determine:
- Whether pre-NEE is correct and fixed NEE is over-estimating
- Whether pre-NEE is under-estimating (convergence issue) and fixed NEE is correct
- Whether both are wrong in different directions

Additionally, fix the `compute_transmittance_to_env` double-counting issue regardless
of the above — filter active primitives from the hit buffer when building the event list:

```cpp
for (const auto& hit : hit_buffer) {
    if (active_prims.contains(hit.prim_idx)) continue;  // already handled above
    // ...
}
```

## Current State

- `ENABLE_NEE = true` in constants.cuh
- Double-counting fix (bounce == 0 escape guard) is in raygen.cuh
- `compute_transmittance_to_env` transmittance bug is known but unfixed
- Mitsuba comparison not yet done
- Brightness difference for dense Gaussians remains unexplained
