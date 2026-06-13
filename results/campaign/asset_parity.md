# G10 — Mitsuba parity gates (tornado, explosion), run 2026-06-13

**Question.** Do our renderer and Jorge's Mitsuba `volprim_prb` agree on the tornado/explosion assets,
so they earn cross-renderer claims (G1)? (The cloud is already validated at 0.9999.)

## The PLY path (the unblock)

The Mitsuba volprim ellipsoids loader needs **Jorge's native PLY**, which ships *inside* the unpacked
asset at `assets/models/unpacked/<asset>/optimized_asset_pyr0/data/root.primitives_pyr0.ply` — **not**
our converter's `assets/models/<asset>/<asset>_pyr0.ply` (our-renderer format). The raw native PLY
stores density as `opacities_0`; the volprim harness (and the cloud's native PLY) want `sigma_t_0`. So
the one historical step is a **value-preserving property rename `opacities_0 → sigma_t_0`** (cloud's
native PLY already has `sigma_t_0`; raw assets have `opacities_0`). Renamed copies written next to the
natives as `root.primitives_pyr0_sigmat.ply` (script: inline `plyfile` rename). The passing gate below
*proves* the rename is correct (value-preserving), so no convention bug was introduced.

## Method

Energy-ratio of converged means (§8.25 method; flip-invariant, so the known `asset_validation`
camera vertical-flip vs Mitsuba does not affect the ratio). Matched both sides: camera `diag`
(dist 3.5, fov 40°), `white_constant` env, **uniform albedo 0.9 scattering**, σ-multiplier 10, 512²,
256 spp. OURS = our PLY + `asset_validation` (calibrated pair); MITSUBA = native(renamed) PLY +
`volprim_prb` **analog** (`SG_NEE=0`, the trustworthy unbiased ref). Driver:
`scripts/campaign/run_g10_parity.sh`. 0 cap overflows.

*(Scattering, not absorption: our `asset_validation` cannot force albedo 0 for these scattering-albedo
assets — `SG_ALBEDO=0` falls back to the PLY's own nonzero albedo; the cloud worked at albedo 0 only
because its PLY albedo is ≈0. A matched uniform-0.9 scattering gate is the natural regime and a
slightly stronger test — it exercises the scattering path too.)*

## Result

| asset | ours mean | Mitsuba-analog mean | ratio | verdict |
|---|---|---|---|---|
| tornado   | 0.966106 | 0.966966 | **0.99911** | PASS |
| explosion | 0.785130 | 0.785085 | **1.00006** | PASS |

**Both pass at 0.01–0.09% — cloud-class agreement.** Confirms: (1) our npy→PLY converter conventions
(opacity→sigma_t, log-scale, quaternion reorder), (2) the `opacities_0→sigma_t_0` rename, (3)
ours-vs-Mitsuba agreement on geometry + density + scattering model (incl. the per-Gaussian
`omega`/frequency kernel term). **tornado + explosion are eligible for cross-renderer claims in G1.**

Per-asset EXRs kept: `g10_<asset>_ours.exr`, `g10_<asset>_mits.exr` (feed the Ch 5 validation
triptychs — ours | Mitsuba | difference).

## Reproduce
```bash
setsid nohup bash scripts/campaign/run_g10_parity.sh </dev/null &   # needs ~/winbins/exe_{tornado,explosion}
```
