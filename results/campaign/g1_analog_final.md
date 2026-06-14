# G1 comparison A (analog vs analog) — re-timed at LOCKED 1800 MHz

ours-analog re-timed at pinned 1800 MHz (median of 5): **2.90 s**/64spp (vs ~9 s Mitsuba-analog).
Variance from banked 16-seed images (consistent 99.9pct firefly-clip both arms):

| arm | k_raw | k_clip(99.9) | t |
|---|---|---|---|
| ours-analog | 15075 | 0.2 | 2.90 s (locked) |
| Mitsuba-analog | 3899 | 110.6 | ~9 s (banked, heavy kernel→clock-robust) |

**The comparison is metric-unstable.** ours samples faster per sample, but its analog variance is
firefly-dominated: raw variance says ours is ~4x NOISIER; firefly-clamped says ours is far CLEANER
(its variance is a few extreme sparse pixels that clamp to ~0.2). No single stable equal-quality number
survives. Conclusion (Ch7 sec:results-perf): the analog sampler is not the decisive factor; the stable,
meaningful equal-quality win is the PRODUCTION MIS (~59x vs Mitsuba's only-unbiased analog). The
architectural contribution is structural (march/sort/root-find-free, faster per sample), not a variance
reduction. Lock confirmed working (SM clock p50/max 1800 during the light kernel).
