# G2b — fast-erf A/B (exact vs approximate erf), run 2026-06-13, 350 W + clock lock

**Setup.** Exact-erf (cloud calibrated **64/96**, `exe_cloud`) vs fast-erf (`build-ferf`, same 64/96 caps,
`THESIS_ENABLE_FAST_ERF=ON`) — only the erf implementation differs (Abramowitz–Stegun rational approx,
~5e-7 erf accuracy, FINDINGS §8.21). Cloud-meadow scattering, `SG_CAM=0`, 64 spp, seed 1, **8 interleaved
rounds** (exact then fast each round; per-round ratio cancels common-mode thermal drift). fast-erf is
numerically ~identical, so **k is unchanged → the win is per-spp time**. Bias confirmed at same-seed 1024 spp.
Driver `scripts/campaign/run_ferf_ab.sh`. Clock sentinel min 1365 / **p50 1635** / max 1800 MHz. 0 overflows.

## Bias (same seed → diff is pure erf error, no MC noise)

| metric | value |
|---|---|
| max\|Δ\| | **1.192e-6** |
| mean\|Δ\| | 3.105e-8 |
| signed-mean Δ | −2.997e-8 |
| mean (exact / fast) | 0.321533 / 0.321533 (equal to 6 digits) |

fast-erf is **unbiased for all practical purposes** — max per-pixel error 1.2e-6 is below the 1e-5
pixel-filter jitter floor and ~100× under the 1e-4 energy gate. k is therefore identical; the equal-quality
speedup equals the per-spp time ratio.

## Timing (per-round exact/fast ratios)

round: 0.993, 1.032, 0.985, 1.018, 0.999, 1.063, 0.973, 1.053 → **median 1.0087 (+0.9 % faster)**

exact median 8.994 s · fast median 8.963 s.

## Finding — small, free, but within the noise floor

fast-erf is **numerically free (max\|Δ\| 1.2e-6) and ~1 % faster**, consistent with the dev-era "~1.5 %"
(§8.21) — but the effect is **at the edge of the per-round jitter** (ratios span 0.97–1.06; 4 of 8 rounds
sit below 1.0). With 8 rounds the magnitude is ~+0.9 % ± ~1.4 % SEM, i.e. **not cleanly separable from
zero**. Honest thesis framing: *fast-erf is a numerically-equivalent ~1 % opt-in win whose exact magnitude
is below this rig's timing resolution* — not the precise "1.5 %" stated as if resolved. It stays **opt-in
(default OFF)** so the validation build keeps exact erf; turn it on for perf-critical runs at no accuracy cost.

**Reproduce:** `setsid nohup bash scripts/campaign/run_ferf_ab.sh </dev/null &` (needs `build-ferf` +
`~/winbins/exe_cloud`). For a publication-tight magnitude, raise the round count to ~30–60 (SEM ∝ 1/√n).
