# G7 — Mitsuba JIT/startup overhead vs OptiX-IR (task #96), measured 2026-06-10

**Method.** Time-to-first-image decomposition, in-process: import+variant, scene build, render #1
(Dr.Jit trace + kernel compile or cache load + launch), renders #2/#3 (steady state). Mitsuba 3.6.4
(Jorge's build, `cuda_ad_rgb`), cloud PLY, 256², 16 spp. Cold cache = `HOME` redirected to an empty
dir (fresh `~/.drjit`, the true first-session cost); warm = the real cache. Our side: `test_runner`
process wall-clock minus its logged render time (PLY + env map + OptiX-IR load + pipeline + AS build),
3 repeats. CPU-dominated → robust at 150 W.

## Results

| | time-to-first-image | startup (excl. steady render) |
|---|---|---|
| **Ours (OptiX-IR, precompiled)** | — | **0.39–0.45 s** (median 0.39) |
| **Mitsuba `volprim_prb` + meadow, warm cache** | 0.85 s | ≈ 0.78 s |
| **Mitsuba `volprim_prb` + meadow, cold cache** | 2.28 s | ≈ 2.20 s |
| Mitsuba `volprim_tomography` (absorption), warm/cold | 0.25 / 1.82 s | ≈ 0.24 / 1.81 s |

Mitsuba per-process JIT delta (prb render #1 − #3): **0.33 s** both warm and cold — the per-process
re-trace dominates; the actual PTX compile is cached effectively.

## Takeaways (honest framing)

- Mitsuba pays **≈ 2× (warm) to ≈ 5.5× (cold)** our startup on this workload — a real but **modest**
  fixed cost (≤ ~2.3 s), amortised away at high spp. It is *not* a major limitation for single-frame
  offline rendering; it matters for short/iterative runs and parameter sweeps (many process launches).
- The Ch 3 "JIT adds launch overhead" claim should be stated at this measured magnitude — the
  *stronger* Mitsuba limitation remains the lack of room for fine-grained, primitive-specific
  optimisation, not startup.
- Steady-state per-frame numbers elsewhere exclude startup on **both** sides (ours: IR load; Mitsuba:
  JIT) — no double-counting in any k·t.

Script: `tools/refs/jit_overhead_timing.py` (INTEGRATOR=prb|tomography; cold via `env HOME=<tmp>`).
