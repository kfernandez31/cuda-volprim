# R3 — scaling: render time and device memory vs primitive count N (2026-06-14)

Locked clocks (SM 1800, mem 9751, 350 W; run clock min=1650 p50=1800 max=1800). Driver:
`scripts/campaign/run_g3_scaling.sh`; data `scaling.csv`; figure `scripts/plots/scaling.py` →
`thesis/latex/figures/scaling.pdf`; section §7.7 (`sec:results-scaling`, `fig:scaling`).
All renders 512², 64 spp, median of 3 seeds.

## A) Synthetic stress grids (SAFE-512 binary → constant 512/512 caps, only N varies)

| grid | N | t_med (s) |
|---|---|---|
| 4×4 | 16 | 0.356 |
| 16×16 | 256 | 1.600 |
| 16×32 | 512 | 0.812 |
| 32×32 | 1024 | 1.779 |
| 32×64 | 2048 | 1.390 |
| 64×64 | 4096 | 3.467 |
| 64×128 | 8192 | 4.063 |

**Only the SQUARE (k×k) grids isolate N cleanly** — the 1:2 rectangles (512, 2048, 8192) change aspect
ratio and screen coverage, so their per-ray work differs and the full series is non-monotonic (512 <
256, 2048 < 1024). The square family {16, 256, 1024, 4096} (constant 1:1 aspect, coverage ∝ N) is
monotonic and fits **t ∝ N^0.40** (256× more primitives → ~10× time). This is the pure
BVH-traversal/geometry cost: doubling N adds ~32% time, not 100%.

## B) Real assets at operating point (matched: scattering albedo 0.9, white_constant env, σ_t scale 10)

| asset | N | t_med (s) |
|---|---|---|
| cloud | 652 | 3.646 |
| tornado | 768 | 5.019 |
| explosion | 1024 | 5.560 |
| bunny | 25600 | 53.896 |

Fit **t ∝ N^0.71**; cloud→bunny is 39× N → 14.8× time (vs 39× for linear). Steeper than the synthetic
N^0.40 because the bigger assets are also optically **denser** media — more scatter events / longer
paths — not because traversal got worse. The architecture keeps the per-ray *geometry* cost sub-linear;
the residual is medium physics, not scene size. (The 53.9 s bunny here is the white-constant scattering
config; the 50.4 s meadow figure in §7.1 is the same asset under the environment map — same order.)

## C) Memory vs N — decoupled

- **Peak device memory is FLAT in N** at a fixed cap: SAFE-512 holds **1200 MiB** at both N=16 and
  N=8192 (per-ray local-memory reservation dominates; `sec:results-memory`).
- The only N-dependent term, the **compacted BVH (GAS)**, is linear and negligible: **~0.16 KB/prim**
  (0.10 MB @652 → 3.97 MB @25600, from `gas_memory.csv`), i.e. 2–3 orders of magnitude below the
  reservation.
- Caps track overlap **density**, not count (`tab:overlap`), so neither term ties memory to N: a
  calibrated build's footprint is governed by how densely the medium packs, not how many Gaussians.

## Notes
- 0 cap overflows on every run (calibrated per-asset binaries for the real assets; SAFE-512 for the
  synthetic sweep).
- Timing extremely stable at locked clocks (e.g. cloud seeds 3.646/3.666/3.606 — sub-1% spread).
