# Furnace energy-conservation gate (B1 evidence) — banked 2026-06-21

Reference-free: albedo=1, white-constant env (value 1), single Gaussian, 1024 spp.
A correct estimator returns the background (mean=1) for any phase/MIS setting.
`centre` = mean over the central 1/4 box (where the in-scatter is). EXRs in results/campaign/furnace/.

| arm | sigma | image mean | centre over-count | verdict |
|---|---|---|---|---|
| ours (MIS+NEE) | 6 | 1.00002 | +0.04% | PASS (flat) |
| Mitsuba analog | 6 | 1.00000 | +0.00% | PASS (flat) |
| Mitsuba NEE | 2 | 0.99606 | +0.86% | FAIL (over-counts) |
| Mitsuba NEE | 4 | 1.00193 | +4.51% | FAIL (over-counts) |
| Mitsuba NEE | 6 | 1.01064 | +9.74% | FAIL (over-counts) |
| Mitsuba NEE | 12 | 1.04932 | +30.94% | FAIL (over-counts) |

**Reading:** ours and Mitsuba-analog conserve energy exactly; Mitsuba-NEE over-counts in-scattered
light at the Gaussian centre, and the over-count grows steeply with optical thickness (~1% -> ~31% as
sigma 2 -> 12). This is the thin-medium signature of the +156% NEE bias on the dense cloud (sec:results-firefly);
FINDINGS 8.1 confirms it is depth-invariant (identical at max_depth 32 and 256, so not a truncation artefact).

## Overlap sweep (Condor round-2 SF2) — the +156% is measured, not argued
`scripts/campaign/run_sf2_overlap_sweep.sh`: Mitsuba-NEE furnace (albedo=1, white-constant) over-count vs
OVERLAP at fixed sigma. Centre = mean over the central 1/4 box. EXRs in results/campaign/furnace/.

| asset | overlap | sigma | image over-count | centre over-count |
|---|---|---|---|---|
| single Gaussian | 1 | 6 | +1.06% | +9.74% |
| Disney cloud | ~45 | 6 | +61.97% | +121.07% |
| Disney cloud | ~45 | 7.5 | +72.57% | +142.44% |

**Reading:** at fixed sigma=6, overlap 1 -> ~45 amplifies the centre over-count ~12x (9.74% -> 121%) from
overlap alone. On the cloud at its own scale 7.5 the furnace (conservative, albedo=1) over-count is already
+142% at the centre, bracketing the +156% of the albedo-0.9 scattering image -- so the +156% is this same
NEE measure-mismatch defect, the small remainder being the albedo-0.9 in-scatter recursion. Bridges the
single-Gaussian thickness trend to the cloud headline with a measurement.
