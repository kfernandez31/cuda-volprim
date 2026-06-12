# G2 — RR-depth sweep (6 depths × 16 seeds), run 2026-06-12

**Setup.** Cloud scattering, meadow, `SG_CAM=0`, **64 spp** (pinned — k is spp-dependent on tail-heavy
envs), MIS (no RIS), seeds 1–16, depths {5,6,8,10,12,16}, interleaved depth-within-seed-block.
350 W, clocks locked 1800/9751 (SM thermally ~1605–1755). 96 renders; per-seed EXRs + times kept in
`results/campaign/rr_seeds/` (gitignored, 595 MB) so a timing-only rerun reuses the images for k.

**Timing contamination + rescue (and the standing rerun note).** A desktop-session burst contaminated
blocks ~8–9 (block means 7.0→16.9 s, recovered by s10). k is image-derived → immune. Times are used as
**per-block-normalized medians** (CV 3–5 %/depth across blocks; t16/t5 = 1.30 ± 0.06 SEM; 3/60
monotonicity violations) with absolutes anchored to the 5 cleanest blocks. **If publication-clean
absolutes are wanted, rerun the timing only (~10 min on a quiet GPU)** — noted in the spec §G2.

## Results (`rr_depth.csv`: t_rel, t_abs_clean, k, k_clip999, eff = k·t_rel)

| depth | t_rel | t_clean | k | eff | eff_clip |
|---|---|---|---|---|---|
| 5  | 0.887 | 6.55 s | 2.461 | 2.183 | 2.077 |
| 6  | 0.911 | 6.77 s | 2.360 | 2.150 | 2.047 |
| 8  | 0.955 | 7.17 s | 2.194 | 2.095 | 1.996 |
| 10 | 1.006 | 7.56 s | 2.073 | **2.085** | **1.987** |
| 12 | 1.063 | 7.94 s | 1.984 | 2.110 | 2.011 |
| 16 | 1.175 | 8.74 s | 1.877 | 2.205 | 2.099 |

k falls monotonically with depth (deeper paths → less termination variance), time rises monotonically;
the efficiency curve is a **shallow basin over depths 8–12** (≤1.2 % spread; nominal minimum at 10),
with clear penalties outside it (5: +4.7 % vs the minimum; 16: +5.8 %). Clipped-k (99.9th-pct
luminance cap) tells the same story.

## Finding — the cited §8.33 magnitude does NOT reproduce

The dev-era claim (FINDINGS §8.33, cited in Ch 6) was "depth-12 ≈ 11 % better quality/sec than
depth-5." This sweep measures **depth-12 only +3.4 % over depth-5** (clipped: +3.2 %), with a timing
SEM of ~2 % — and depth-10 marginally ahead of 12. What survives: the *direction* (raising 5→12 helps,
never hurts; 12 sits inside the optimal basin) and therefore the shipped default. What does not: the
~11 % magnitude, at this operating point (64 spp, meadow, MIS, 350 W). Plausible sources: different
dev-era operating point/GPU state, different spp, and the basin being shallow enough that small
condition changes move the apparent gain.

**Ch 6 action (queue for the Ch 6 pass):** `fig:rr-depth` plots eff from `rr_depth.csv`; soften the
5→12 claim to "a shallow efficiency basin spans depths 8–12 (~3–5 % over the extremes); 12 was kept"
and drop/replace the 11 % figure. tab:wins row, if it quotes 11 %, needs the same edit.

## Reproduce

```bash
for s in $(seq 1 16); do for d in 5 6 8 10 12 16; do
  SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering \
    --spp 64 --seed $s --rr-depth $d   # cp EXR per (d,s); log Total time
done; done
# k: per-pixel inter-seed variance across the 16 seeds, mean over pixels+channels, ×64 (spp)
# eff: k × median per-block-normalized time
```
