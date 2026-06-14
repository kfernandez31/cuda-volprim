# G1 comparison A — ours-analog vs Mitsuba-analog (2026-06-14)

Pure-analog fix validated (furnace PASS; cloud-meadow mean 0.327 ≈ Mitsuba-analog 0.320). Comparison A
(both analog, no NEE, isolates the sampler from the direct-lighting estimator):

| arm | raw k (var×spp) | mean | t_med |
|---|---|---|---|
| ours-analog | 15075 | 0.3276 | 2.95 s |
| Mitsuba-analog (banked g1) | 3899 | 0.3201 | ~9 s |

**Core sampler ≈ on par at equal quality.** Ours samples ~3× faster per sample (single-trace argmin,
no shadow rays / root-find) but carries ~4× the raw per-pixel variance (firefly-dominated), so
equal-quality (k·t) is roughly neutral (~0.8× raw). Caveats: clocks throttled during the light analog
kernel (min 1065, p50 1665 MHz); RR/firefly-clamp configs not matched to Mitsuba-analog; clipped-k not
reconciled to the banked firefly-clamp metric. The number is therefore reported qualitatively ("on
par"), not as a precise figure.

**Conclusion / thesis framing (Ch7 sec:results-perf):** the decisive equal-quality win is the
*production* 59× — driven by ours having correct low-variance MIS while Mitsuba's only unbiased mode is
the firefly-noisy analog. The core sampler's contribution is *structural* (march/sort/root-find-free,
~3× faster per sample), not a per-sample variance reduction. Honest "both numbers": 59× production
(MIS vs Mitsuba's-only-unbiased analog) + core-sampler on-par (analog vs analog).
