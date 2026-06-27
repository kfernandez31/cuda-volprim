# Sampler-only convergence: ours-analog vs Mitsuba-analog (FLAT env) — 2026-06-26

Both in analog mode (no NEE/MIS) -> isolates the SAMPLER. Flat (white-constant) env, cloud, sigma 7.5,
albedo 0.9, HG 0.85, BOX filter on BOTH (matched), 6-spp ladder {16..512}, 8 seeds. Flat removes fireflies
so raw per-pixel inter-seed variance is the clean metric.

| spp | ours var | mits var | ours/mits | ours t(s) | mits t(s) |
|---|---|---|---|---|---|
| 16  | 3.64e-3 | 2.30e-3 | 1.58 | 0.57 | 2.85 |
| 64  | 9.12e-4 | 5.74e-4 | 1.59 | 2.36 | 11.31 |
| 256 | 2.28e-4 | 1.44e-4 | 1.59 | 9.73 | 45.33 |
| 512 | 1.14e-4 | 7.19e-5 | 1.59 | 19.58 | 90.58 |

- Variance: ours-analog **~1.6x noisier** per pixel (flat, constant across spp; slope -1 both).
- Time: ours **~4.6x faster per sample** (clean; mits 512 seeds 4-7 were GPU-contended @288-339s and dropped;
  clean mits 512 = 90.6s).
- **NET equal-variance: ours-analog ~2.9x MORE efficient** (E=var*time: ours 2.22e-3 vs mits 6.51e-3 at
  256spp -> 2.94x; 2.92x at 512). Ours WINS.

## IMPORTANT: this REVISES the thesis flat-env claim (net ~0.83x, "sampler is faster-but-noisier")
The thesis's "ours-analog ~5x noisier, net 0.83x (Mitsuba ahead)" was a **filter mismatch**: the earlier
flat scripts (run_g1_flat.sh, run_flat_timing_pin.sh) set NO SG_RFILTER, so Mitsuba used its DEFAULT
GAUSSIAN reconstruction filter (which reduces per-pixel variance ~3x) while ours always uses BOX. ours(box)
vs mits(gaussian) is not a fair sampler comparison. With matched BOX on both (this run), ours-analog is only
1.6x noisier and is net ~2.9x MORE efficient. (A matched-GAUSSIAN run would preserve the ratio, since the
filter benefits both samplers equally; the mismatch is what produced 0.83x.)

=> The sampler itself is a NET WIN on flat (~3x), not a wash. Recommend revising the §7 "isolating the
sampler" paragraph: ours-analog is faster per sample AND net more efficient at matched filter; the earlier
0.83x is withdrawn as a box-vs-gaussian artifact. (Validate with a matched-gaussian A/B if desired.)
Plot: thesis/latex/figures/analog_convergence.pdf

## POWER DEPENDENCE (2026-06-27, Prybicki enforced 150W) — time ratios are NOT power-invariant
Interleaved ABAB, 256spp, power+clock logged, round-1 (cold ramp) dropped:
| power | mits/ours time ratio | net equal-variance (var ratio 1.59) |
|---|---|---|
| 350W | 4.57x (4.70/4.57/4.55) | 2.87x |
| 150W | 5.61x (5.69/5.61/5.58) | 3.53x |
Mitsuba (Dr.Jit) throttles HARDER than our megakernel under the power cap, so 150W WIDENS our lead.
=> Time-based ratios MUST be reported at the operating power. Variance/bias results are power-independent
(unchanged). At Prybicki's enforced 150W, ours-analog net efficiency is ~3.5x (better than 350W's ~2.9x).
Absolute renders are ~2.8x slower at 150W (ours 256spp 9.8s->27.5s; mits 45.3s->155s).
IMPLICATION: re-anchor all ours-vs-Mitsuba TIME numbers to 150W (they improve for us); the 59x equal-
quality headline is variance-dominated so it holds as a floor (the time factor shifts in our favour too).
