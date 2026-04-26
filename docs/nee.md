# Next Event Estimation (NEE) Plan

Reference impl: `feature/test-suite` — commits `d19ea98` (add NEE) and `675b88b` (fix double-counting).

## Why
At each scatter point, sample the environment directly via a shadow ray. Cuts variance dramatically for thin/medium media because most paths reach the env eventually but few reach it after just one bounce. With isotropic phase + constant env, NEE ≈ 1-bounce convergence.

## Where it goes
`device/core/sampling.cuh`. After `sample_scattering_event` returns true, before sampling the next phase direction.

## Signature
```cpp
__device__ float3 compute_transmittance_to_env(
    const geometry::Ray& shadow_ray,
    const PrimsSet& active_prims,
    HitBuffer& hit_buffer);
```
Reuse `compute_escape_optical_depth` semantics — collect entry hits along shadow ray, build entry+exit events for active prims and hit prims, sort, march, return `exp(-τ)`.

## Integrand change
Replace at scatter point:
```cpp
radiance += throughput * albedo * env * PHASE_VALUE;          // before
radiance += throughput * albedo * env * PHASE_VALUE * T_nee;  // after
```
where `T_nee = compute_transmittance_to_env(shadow_ray, active_prims_at_scatter, scratch_hits)`.

## Critical bug (per `675b88b`)
If a path scatters and you also accumulate the escape contribution unconditionally, you double-count. Either:
- Only add escape contribution when `sample_scattering_event` returns false, OR
- Use MIS weights between NEE and the BSDF/free-flight escape.

Single-sample NEE with no MIS is fine for now (constant env, isotropic phase → MIS weight degenerates to 1/2 at most).

## Toggle
```cpp
constexpr bool ENABLE_NEE = true;  // in constants.cuh
```
Keep it compile-time so the dead path is DCE'd; saves registers when off.

## Cost
+1 ray + 1 segment-march per bounce. Expect ~2× per-bounce time but ~4–10× SPP-equivalent variance reduction on this cloud → net win.

## Validation
Render at fixed seed, NEE on vs off, check (1) mean radiance matches within 1σ, (2) variance drops, (3) RMSE vs `ref-better.exr` improves.
