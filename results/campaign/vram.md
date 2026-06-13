# G5b — peak VRAM (the memory gap), 2026-06-13

Per-process peak device memory (`nvidia-smi --query-compute-apps=used_memory`, polled @150 ms during a
16-spp render; VRAM reservation is power-independent → 150 W). Driver/data: `scripts/campaign/run_g5b_vram.sh`,
`results/campaign/vram.csv`, `g5b_vram.log`. The dominant term is the **per-ray local-memory reservation**:
`HitBuffer` (`raygen.cuh:107`) is a per-thread stack buffer of `6 B × HIT_BUFFER_CAPACITY` plus the
`CompactSet` (`2 B × MAX_ACTIVE_PRIMS`); CUDA reserves it for the max resident threads, so it scales with
the compiled caps and is essentially asset-independent at a fixed cap. The asset-dependent GAS is tiny by
comparison (cloud 0.10 MB, bunny 3.97 MB compacted — `gas_memory.csv`).

## Per-asset peak VRAM: calibrated vs SAFE-512

| asset | caps (active/hit) | calibrated | SAFE-512 | saved | saved % |
|---|---|---|---|---|---|
| cloud | 64/96 | 578 MiB | 1200 MiB | **622** | 51.8% |
| tornado | 112/384 | 818 MiB | 1200 MiB | **382** | 31.8% |
| explosion | 32/160 | 600 MiB | 1200 MiB | **600** | 50.0% |
| bunny | 80/528 | 900 MiB | 1200 MiB | **300** | 25.0% |

**SAFE-512 is a flat 1200 MiB regardless of asset** — the conservative 512/512 sizing reserves the same
worst-case local-memory pool for every scene, confirming the reservation (not the geometry) dominates.
Per-asset calibration cuts this to each asset's true demand, saving **0.30–0.62 GiB** (25–52%). The
payoff tracks the hit-cap: bunny (528) is already near 512 so it saves least; cloud (96) saves most.
This quantifies the cap-calibration work's memory benefit, previously unmeasured.

## ours vs Mitsuba (cloud, meadow, 900×600)

| renderer | peak VRAM |
|---|---|
| ours (calibrated 64/96) | **578 MiB** |
| ours (SAFE-512) | 1200 MiB |
| Mitsuba (volprim, analog) | 838 MiB |

Calibrated, ours uses **31% less** device memory than Mitsuba on the same scene; uncalibrated (512/512)
it uses ~43% more — so the cap calibration is what puts the renderer below the reference on memory, not
just on time.

## Notes
- 0 cap overflows on every run (calibrated caps validated again here).
- 16-spp renders used (VRAM reservation is independent of spp/seed — set at first launch by the buffer
  sizes and grid); resolutions are each asset's render resolution (cloud native 900×600, others 512²).
- Mitsuba figure polled GPU-wide (single compute app; GPU otherwise idle).
