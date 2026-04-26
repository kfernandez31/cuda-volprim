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

## Other deferred items

(Add new items above this line.)
