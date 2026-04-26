# Deferred Work

Items intentionally left for later, with enough context that a future pass can pick them up cold.

---

## Adaptive sampling — fix properly

**Status:** disabled via `ADAPTIVE_THRESHOLD = 0.0f` in `device/core/constants.cuh`. Kernel still pays the cost of loading `mean`/`M2` and running the convergence check at launch start, but the threshold is unreachable so every pixel gets full SPP.

**Why disabled:** the existing implementation produces a visible cutoff line on most renders — a sharp boundary between converged-and-frozen pixels and still-being-sampled neighbors. Three structural problems compound:

1. **Binary, irreversible convergence.** Once a pixel meets the threshold it stops sampling for the rest of the render. Neighbours that didn't converge keep getting smoother, creating the visible boundary.
2. **No spatial regularisation.** Convergence is decided per-pixel from `ADAPTIVE_MIN_SAMPLES = 32` draws, which is enough draws for the variance estimate itself to vary noticeably between adjacent pixels. A pixel can freeze at the wrong value purely by chance. PBRT v4 / Mitsuba mitigate this by averaging variance over a 3×3 neighbourhood.
3. **`sqrt(M2/(n-1))` is biased low at small n.** By Jensen's inequality the std-dev estimate is systematically below the true value, so the criterion fires too eagerly. Bias is ~1.5% at n=32 but compounds with (2).

**Proper fix (in priority order):**
- Spatial variance filter: average M2/(n-1) over a 3×3 (or 5×5) neighbourhood before computing the relative-error criterion. Requires either a separate pass or shared-memory cooperation between pixels.
- Hierarchical refinement: render a low-SPP base pass (~8–16 SPP), compute a per-tile or per-pixel error map, then drive a *budget* of additional samples toward high-error pixels. Each pixel can resume sampling — never frozen permanently.
- Correct the std-dev bias for small n with the c4 correction factor (or just use M2 directly without sqrt — relative variance threshold instead of relative std-dev).
- Ground-truth metric: keep a separate "reference" path with very high SPP for a small subset of pixels and validate the adaptive estimator's relative error against it.

**Validation criteria for the fix:**
- No visible cutoff line at any threshold setting between 0.001 and 0.05.
- Equal-time renders with adaptive on vs off should have RMSE_adaptive ≤ RMSE_uniform vs reference (the whole point is variance reduction at fixed budget).
- Pixels that look "converged" early in the render *can resume sampling* if their estimate drifts.

**Reference reading:**
- PBRT v4 §16.5 (adaptive sampling).
- Mitsuba `PathIntegrator` adaptive variant.
- Robust Image Denoising (Chaitanya 2017) — discusses related variance-aware sampling.

**Estimated cost:** 2–3 focused days. Most of the work is the spatial variance filter + plumbing the per-pixel sample-budget mechanism, not the math.

---

## ADT scatter sampling for hit-buffer primitives — segment-restricted inv_cdf

**Status:** Currently using full-Gaussian inv_cdf with rejection sampling for primitives the ray enters mid-flight (those in `hit_buffer`). Subtly biased.

**Where:** `device/core/sampling.cuh::sample_scattering_event`, the second per-primitive loop:
```cpp
for (const auto& hit : hit_buffer) {
    const float t_scatter = prim.inv_cdf(ray, tau_j);
    if (t_scatter >= hit.t_hit && t_scatter < t_scatter_min) { ... }
}
```

**The bug:** ADT (SDTracking §4.1, Theorem 1) requires each per-primitive free-flight sample `T_i` to come from medium `i`'s **own** CDF along the ray — i.e., the integral of σ_i(t) starting at t = 0. For a primitive the ray hasn't entered yet, σ_i(t) = 0 for t < t_hit and Gaussian for t ≥ t_hit. The correct sample solves `∫_{t_hit}^t σ(s) ds = tau_j`.

The current code samples from the *full* Gaussian CDF starting at t = 0 in primitive's local frame and rejects any result with `t_scatter < hit.t_hit`. Rejected samples are dropped (not re-rolled), so the primitive systematically under-contributes scatter events.

**Bias direction:** scatter events are pushed further along the ray (or replaced by escape). Rendered images are biased toward less scattering → brighter for high-albedo media, more transmissive for low-albedo media. Invisible on the absorber (no scattering anyway); affects scattering scenes by an unknown amount.

**Why we haven't seen it explicitly:** no scattering reference to compare against. Absorber RMSE doesn't exercise this code path.

**The fix:** add a segment-restricted variant of `inv_cdf` to `primitive.h`. Math is the same closed-form erf inversion already used in `optical_depth(ray, t0, t1)`, just rearranged to solve for `t_scatter` given `tau`. Roughly:

```cpp
// Solve: optical_depth(ray, t_hit, t_scatter) = tau_target
// for t_scatter ∈ [t_hit, t_exit].
__device__ float inv_cdf_segment(const Ray& ray, float t_hit, float tau_target) const;
```

Then in `sample_scattering_event`, replace `prim.inv_cdf(ray, tau_j)` with `prim.inv_cdf_segment(ray, hit.t_hit, tau_j)` for the hit-buffer loop. Drop the `t_scatter >= hit.t_hit` rejection — it becomes implicit.

**Validation:**
- Render the absorber → RMSE 0.0504 should not change (no scattering, no path through this code).
- Render the scattering scene → may change visibly (less bias).
- Compare against Mitsuba reference once available for a scattering scene.

**Estimated cost:** half a day. The math is closed-form (single erf inversion); the call-site change is mechanical.

---

## Other deferred items

(Add new items above this line.)
