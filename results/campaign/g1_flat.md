# G1 FLAT rung — ours-MIS vs Mitsuba-analog on a FLAT (white-constant) env (2026-06-14)

Bounds how much of the meadow ~59x is **environment importance sampling**. On a flat (uniform) env
there is no peakiness for our MIS to exploit AND no bright-sun fireflies to plague Mitsuba-analog.
8 seeds @64 spp, sigma 7.5, albedo 0.9, HG g=0.85, cloud native res. Clocks pinned (held ~1620 p50
under load). EXRs verified distinct per seed (8 unique md5s — renders genuinely ran).

## Result
- ours-MIS (flat)     : mean=0.6212  k_clip999=0.10  t_med=7.71s (n=8)
- Mitsuba-analog(flat): mean=0.6213  k_clip999=0.01  t_render~8.5s (wall ~11.3s incl. startup)
- **mean ratio ours/mits = 1.0000** — correctness holds on flat env (the two agree to 0.02%).

## Equal-quality (the point of the rung)
- Equal render time, variance ratio (ours k / mits k) = **10x** — Mitsuba-analog is ~10x LOWER noise.
- Equal-quality speedup ours-over-mits = **0.11-0.15x** -> **Mitsuba is ~7-9x better at equal quality.**

## Interpretation (honest)
The meadow **~59x is ENTIRELY environment-importance-sampling**: it is the value of MIS-sampling a peaky
HDR, not a property of the sampler/architecture. Remove the peaky env and the advantage not only vanishes
but **inverts** — under flat lighting our MIS does per-vertex NEE (shadow rays with stochastic
transmittance) that adds variance for no benefit, while Mitsuba-analog on a constant env is nearly
noiseless (every escape returns the same constant). So ours-MIS is ~10x noisier here.

Caveat / fair reading: on a flat env one would not *use* MIS on our side either — analog is the right
mode there, and ours-analog would roughly match Mitsuba-analog (both pure random walks). The rung is not
"Mitsuba beats us on flat"; it is "**the 59x headline is environment-lit-specific** — it is the payoff of
importance-sampling the environment, and it should be reported as such, not as a generic sampler win."
This is consistent with the RIS finding (§8.37): the env-IS machinery helps under peaky illumination and
hurts on flat.

## STABLE core-sampler comparison (ours-ANALOG vs Mitsuba-analog, flat) — the number meadow couldn't give
On meadow the analog-vs-analog comparison was firefly-metric-unstable (no stable figure). On a FLAT env
there are no fireflies (k_raw == k_clip on both sides), so the same comparison is finally STABLE:

- ours-ANALOG flat   : mean=0.6212  k_clip=0.058 (raw 0.058)  t_med=**2.85s**  (n=8)
- Mitsuba-analog flat : mean=0.6213  k_clip=0.012 (raw 0.012)  t_render~**8.5s** (n=8)
- mean ratio = 1.0000 (correctness ✓, unbiased)

Two clean, stable facts:
1. **Per-sample throughput: ours is ~3x faster** (2.85s vs ~8.5s at the SAME 64 spp) — the single-trace
   argmin needs no per-segment root-find, no segment march, no shadow connection. This is a concrete
   number behind Ch7's qualitative "faster per sample" claim.
2. **Per-sample variance: ours is ~5x HIGHER** (k 0.058 vs 0.012; both unbiased) — so the net
   equal-quality on flat is **~0.6x (Mitsuba ~1.7x ahead)**: our speed does not fully offset the higher
   analog variance.

Interpretation: the architectural contribution is a **structural/throughput simplification (~3x faster
per sample), not a per-sample variance reduction** — exactly the framing already in Ch7
(sec:results-perf). The production win is the correct, low-variance MIS estimator under peaky env
lighting (59x); the bare sampler is faster-but-noisier. (Aside: ours-analog being ~5x noisier per sample
than Mitsuba-analog, both unbiased on identical geometry, is an efficiency gap worth a footnote — likely
RR/path-length-distribution differences; not a correctness issue since means match.)
