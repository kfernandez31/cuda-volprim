# Scattering voxel-GT via AdVol — SOLVED (2026-06-14)

The previous voxel-GT attempt got a clean ABSORPTION cross-check but scattering was firefly-limited
(stock Mitsuba global-majorant delta tracking, clamp400 RMSE 1.13, analog-like k≈3899). **AdVol**
(`~/jorge/advol`, Jorge's DSYG grid baseline: a drop-in Mitsuba integrator with swappable distance
samplers / transmittance estimators) has the machinery stock Mitsuba lacks — **local supervoxel
majorants + residual ratio tracking** (`ff_local` / `rrt_local`, DDA-traversed). That fixes it.

## Setup
`tools/refs/voxel_gt_advol.py`: our cached cloud grid (`voxgrids/cloud_200_ss1.npz`, peak σ_t≈8892,
UNCLAMPED) → `advol.VolumeData.from_array` → `advol.build_medium(albedo=0.9, phase=hg g=0.85,
supervoxel_factor=4, majorant_factor=1.01)`, exact cloud scene (cam_0000), meadow envmap (roty 90),
integrator `{advol, ff_local, rrt_local, max_depth=128}`. Variant `cuda_ad_rgb` in `tools/refs/.venv`
(advol already `pip -e` installed there). Run at 150 W — this is a variance/RMSE check, clock-independent.

## Result — clean, unclamped, firefly-free
| metric | stock attempt (prior) | AdVol (this) |
|---|---|---|
| clamp | clamp 400 (and still bad) | **UNCLAMPED** (peak 8892) |
| max pixel | ~1000+ | **2.5** (256 spp) / 3.8 (64 spp) |
| fireflies (>20× mean) | ~1300/img | **0** |
| inter-seed k (64 spp, 4 seeds) | analog k≈3899 | **k_raw 0.738 ≈ k_clip 0.721** (no tail) |
| mean vs ours (0.3214) | ~6% (clamped) | **+1.5%** (0.3261, unclamped) |
| per-pixel RMSE vs ours-converged | 1.13 | **0.114** (256 spp, converged) |

~3 orders of magnitude lower per-pixel variance; ~10× lower RMSE; and no clamp needed. The residual
RMSE 0.114 is resolution-limited (200³ blurs the sub-voxel cores), the same character as the absorption
GT (RMSE 0.063 at 200³) — NOT noise, NOT fireflies.

## Thesis
Ch5 `sec:voxel-gt` scattering paragraph rewritten (was "firefly-limited, mean-only") + new
`fig:voxel-scatter-gt` (ours | AdVol grid GT | |diff|×5). Scattering now has the same independent,
no-shared-code corroboration as absorption. Scripts: `tools/refs/voxel_gt_advol.py`,
`scripts/plots/voxel_scatter_gt.py`. (Minor: a couple of small dark supervoxel-boundary specks in
low-density regions of the grid render — cosmetic, does not affect mean/RMSE.)

## Still gated
Representation-fidelity GT (our fit vs the ORIGINAL Disney WDAS volume) still needs the source
`wdas_cloud_eighth.npy` — referenced by `advol/examples/wdas_cornell.py` but not found on disk.
