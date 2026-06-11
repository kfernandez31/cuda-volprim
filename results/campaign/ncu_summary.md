# ncu profile — render megakernel (current binary, 2026-06-10)

**Setup:** `cloud_asset_scattering` (652-prim cloud, meadow HDR, MIS, 16 spp, scene-native 900×600),
RTX 3090. `ncu` controls clocks itself (base clock), so these metrics are **final** — independent of
the 150 W cap. Invocation per FINDINGS §8.28 recipe; kernel `regex:optixLaunch`, 1 launch.

## Speed of Light
| Metric | Value |
|---|---|
| Compute (SM) throughput | **45.9 %** |
| Memory throughput | 37.3 % |
| DRAM throughput | **16.5 %** (≈ 150 GB/s of 936) |

Neither compute nor DRAM saturated → not compute-bound, not bandwidth-bound.

## Occupancy / scheduler
| Metric | Value |
|---|---|
| Achieved occupancy | **31.2 %** (15.0 warps/SM) |
| Registers/thread | **114** |
| Issued warp per scheduler | 0.47 |
| Eligible warps per scheduler | **0.72** |
| No-eligible (scheduler idle) | **52.9 %** |
| Warp cycles per issued instruction | 7.98 |

Scheduler starved half the time → latency-bound.

## Divergence (the headline of this profile)
| Metric | Value |
|---|---|
| **Avg active threads per warp** | **6.95 / 32** |
| Avg not-predicated-off threads | 6.60 |

Pipes are busy ~46 % of issue slots but only ~22 % of lanes are alive → effective FLOP throughput is
~2.8 % of peak. This is the quantified ray-divergence cost (the SER-on-Ada discussion's basis).

## Roofline point (estimate; see method)
| Quantity | Value |
|---|---|
| DRAM traffic | 335.89 GB over 2.24 s → 150 GB/s |
| FMA-pipe warp-instructions | 217.59 e9 (xu/transcendental: 5.56 e9) |
| FLOPs (est.) | 217.59e9 × 6.95 active threads × ~1.5 FLOP/inst ≈ 2.27 TFLOP |
| Achieved | **≈ 1.0 TFLOP/s** (2.8 % of 35.6 TFLOP/s FP32 peak) |
| Arithmetic intensity | ≈ 6.75 FLOP/B |

Method note: OptiX kernels reject SASS-patching FLOP counters (returned 0), so FLOPs are estimated
from pipe counters × average active lanes × an FFMA/FADD mix factor (1.5); transcendental (erf) work
runs on the XU pipe and is NOT counted as FLOPs — the GFLOP/s figure *undercounts* the kernel's real
arithmetic. The roofline is therefore presented strictly as a **non-saturation** argument (the point
sits far below both roofs), per the plan-review framing.

## Deltas vs FINDINGS §8.28 (different config — expected)
§8.28 profiled `asset_validation` (bunny) @256²: 21.7 % occupancy, SM 34.5 %, DRAM 1.1 %. This profile
is the cloud + meadow + MIS scattering showcase at 900×600 on the post-RIS binary: occupancy 31.2 %,
SM 45.9 %, DRAM 16.5 % (env-map fetches + larger grid). Same qualitative verdict in both: pure
latency-bound, scheduler-starved, divergence-dominated.

## Full setup pin (run log)

- **Binary:** `build/bin/Release/test_runner` built 2026-06-08 from `d44ad95` (post-RIS-merge); no
  device/host source changes since (working tree clean for device/include/src). Release: `-O3`,
  `--use_fast_math` (+`THESIS_ENABLE_FAST_MATH`), **exact erf** (`THESIS_ENABLE_FAST_ERF=OFF`),
  precompiled OptiX-IR.
- **Compile-time buffer constants (stock, cloud-tuned):** `MAX_PRIMITIVES=1024` (→ CompactSet active
  set), `MAX_ACTIVE_PRIMS=128`, `HIT_BUFFER_CAPACITY=128`. Cloud worst case is 45/96 (estimator), so
  no overflow; no overflow-counter warning fired in any profiled run.
- **Render config:** σ-multiplier 7.5, PLY albedos, HG g=0.85, MIS (RIS off), max-depth 128, RR from
  12 (max survival 0.99), no firefly clamp, box filter, no denoiser, seed 42, 16 spp (single launch),
  camera cam_0000, scene-native 900×600 → grid (240,19,1)×(128,1,1) = 583,680 threads.
- **Stack:** driver 580.95.05, CUDA 12.6 (V12.6.20), ncu 2024.3.0; ncu base-clocked
  (`--clock-control` default), GPU otherwise idle.
