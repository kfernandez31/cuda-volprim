# TODO — renderer optimization (after scattering validation)

Scattering is validated end-to-end (FINDINGS §8: furnace → single → clusters → cloud,
agreeing with Mitsuba analog to ≤~1e-4). The cloud RMSE is pure MC noise, NOT error.
Remaining work is EFFICIENCY (lower RMSE per spp + faster), plus a tiny open residual.
Two axes: variance (lowers RMSE AND equal-quality time) and raw throughput.

Measured baseline (cloud cam0, σ=7.5 albedo=0.9, FINDINGS §8.5):
- CUDA noise const kC=0.411 ; Mitsuba kM=0.243  (CUDA 2.85× noisier per sample, on CONSTANT env)
- throughput: CUDA 0.934 s/spp ; Mitsuba 0.484 s/spp  (1.93× slower per sample)
- equal-quality: CUDA ~5.5× slower

## B. Throughput (do FIRST — low risk, no correctness re-validation)
- [ ] **B1 — right-size per-ray buffers.** HitBuffer 128×8B=1KB + EventBuffer 256×8B=2KB
      + CompactSet 128×2B ≈ 3.3KB/thread of LOCAL memory → spills → low occupancy.
      Code comment proves it: 128→256 was ~6× slower. Cloud simultaneous overlap is
      37–45, so 128 is likely oversized. ACTION: instrument actual max-hits-per-ray for
      the cloud, then lower HIT_BUFFER_CAPACITY / MAX_ACTIVE_PRIMS to fit (e.g. ~80).
      RISK: silent drop if undersized (cap-overflow gotcha) → MEASURE before cutting.
      Files: device/core/constants.cuh (HIT_BUFFER_CAPACITY=128, MAX_ACTIVE_PRIMS=128).
- [ ] **B2 — NSight Compute pass** (never done): occupancy, register pressure, local-mem
      traffic, warp divergence. Get real numbers before deeper changes.
- [ ] **B3 — megakernel → wavefront** (LAST RESORT, big rewrite): split intersect/scatter/
      shade kernels to cut register pressure. Only if B1/B2 insufficient.

## A. Variance reduction (lowers RMSE directly + closes most of the speed gap)
- [ ] **A1 — per-step Rao-Blackwellization (the big lever, ~2.85×).** Mitsuba folds analytic
      segment transmittance into throughput at EVERY bounce (β*=seg_tr); CUDA only does it
      at bounce 0 (ENABLE_ANALYTIC_DIRECT). Extend to all bounces.
      CAVEATS: (1) fights the ADT-argmin novelty (RB needs ordered segment march — the
      thing argmin avoids → may COST throughput per sample; measure NET equal-quality).
      (2) The 2.85× is on CONSTANT env; on a real env-map the NEE/analog ranking may flip,
      so measure variance on the TARGET lighting first. (3) MUST re-validate furnace +
      full ladder — a variance-reduction bug that silently biases is exactly Mitsuba's
      NEE +6.5% trap. Files: device/core/sampling.cuh (sample_scattering_event,
      compute_transmittance_to_env), device/entry/raygen.cuh (bounce loop).
- [ ] **A2 — MIS + env importance sampling** (ENABLE_MIS=false today). NO benefit on
      constant env; real win only for a REAL env-map beauty shot. Defer to beauty stage.

## Open correctness item (tiny)
- [ ] **Traits overlap residual** (FINDINGS §8.3): +0.0002 CUDA-brighter in dense overlap
      core, scattering-only. Below detection at cloud scale. Candidate: NEE shadow-ray
      transmittance from a scatter vertex INSIDE overlapping prims (exit_from_inside on
      several prims). Revisit if it surfaces once noise floor is driven lower.

## Other deferred
- [ ] uint16 spp ceiling (Image::sample_counts_ wraps at 65536) → widen to uint32.
- [ ] Cloud scattering: only cam 0 rendered; do more cameras once converged.
- [ ] Pick the scattering "look" (albedo/σ) — design choice, NOT constrained by refs/
      (refs/ are absorption; see memory reference_asset_density_scales).
