# G8 — Analytic sphere vs tessellated icosphere: port + accuracy/memory axes (2026-06-11)

Branch `feature/icosphere-gas`. Resurrects the historical icosphere geometry (commit `eb5372f`)
behind a compile-time toggle so the renderer can swap its analytic OptiX built-in sphere for a
tessellated icosphere triangle GAS — a fair A/B where **only the per-primitive geometry differs**
(IAS instancing, any-hit entry collection, analytic optical-depth exit all unchanged). Benchmarks our
analytic choice directly against the reference's (DSYG) tessellated approach.

## Implementation (compile-guarded, default OFF = analytic)

Toggle: `cmake -DTHESIS_ICOSPHERE=ON -DTHESIS_ICOSPHERE_N=<0..3>` (default OFF). Files (all guarded by
`THESIS_ICOSPHERE`, so the production build is byte-unaffected — verified: default rebuild + render
unchanged):
- `include/thesis/host/geometry/mesh.h` (new) — `Icosphere<N>` (12→642 verts), rewritten glm-free on
  the project's `float3` math (the tree no longer depends on glm).
- `include/thesis/host/optix/gas.h` — `IcosphereGAS`, a drop-in for `SphereGAS` with the *same*
  `(ctx, stream)` ctor + `build()`/`get()`, so the swap is a one-line typedef.
- `include/thesis/host/app/renderer.h` — guarded member type (`IcosphereGAS` vs `SphereGAS`).
- `src/thesis/host/app/renderer.cpp` — pipeline `usesPrimitiveTypeFlags` TRIANGLE vs SPHERE; skip the
  built-in-sphere IS module; pass `nullptr` IS to the hitgroup (hardware triangle intersection).
- `device/entry/anyhit.cuh` — **front-face filter**: a ray crosses each convex icosphere through a
  front (entry) and a back (exit) face; we keep only the entry (`optixIsTriangleBackFaceHit() →
  ignore`) so each primitive is collected once, exactly as the single-hit built-in sphere is. The exit
  stays analytic (`compute_exit_from_entry`).
- `cmake/Device.cmake` + `cmake/OptiX-IR.cmake` — declare the option; thread the define to host
  (PUBLIC on `device`, like `THESIS_ENABLE_FAST_MATH`) and to the OptiX-IR shader compile.

The unit icosphere is mapped to each Gaussian's 3σ ellipsoid by the *same* per-instance `localToWorld`
the analytic unit sphere uses, so the IAS (652 instances) is geometry-independent and unchanged.

## Validation — accuracy vs the analytic render (the exact ground truth), THREE assets

The primitive *is* a sphere, so the analytic render is exact and the discrepancy at each `N` is pure
faceting error. Absorption, `white_constant`, 64 spp (transmittance deterministic). Tested on **cloud,
tornado, and bunny** (built at 320/496, which fits all three → 0 overflow drops, so the diff is pure
faceting, not truncation). RMSE vs the analytic render (`results/campaign/icosphere.csv`; GAS size is
asset-independent — the unit icosphere is instanced):

| N | verts | tris | cloud RMSE | tornado RMSE | bunny RMSE | signed-mean (sign) | GAS |
|---|---|---|---|---|---|---|---|
| 0 | 12  | 20   | 4.01e-2 | 9.39e-3 | 1.18e-2 | − (brighter) | 1408 B |
| 1 | 42  | 80   | 1.01e-2 | 1.95e-3 | 2.48e-3 | − (brighter) | 3328 B |
| 2 | 162 | 320  | 2.54e-3 | 4.57e-4 | 5.84e-4 | − (brighter) | 9600 B |
| 3 | 642 | 1280 | **8.19e-3** | **4.59e-3** | **4.43e-3** | **+ (darker)** | 42 496 B |

**N=0→2 is a clean ~4×-per-subdivision faceting curve on all three assets**, with the physically
correct sign: the inscribed icosphere (vertices on the sphere, faces chord-inside) is slightly
*smaller* than the true ellipsoid → less absorption → *brighter* → negative signed-mean. This proves
the port is correct (a wrong front-face filter would double-count → tens-of-percent error everywhere;
instead it's <0.3% mean, edge-localized, and converges).

**N=3 reverses on every asset** — RMSE grows and the signed-mean flips to *darker* (positive). It is a
**localized** high-error population, not a global bias: at |Δ|>0.05 the count jumps from ~0 at N=2 to
cloud 3170, tornado 475, bunny 185 px at N=3. Cause: at fine tessellation the icosphere triangles
become small slivers; near-grazing rays land a faceted entry point far from the true surface, and
"analytic exit from faceted entry" then mis-integrates a handful of rays. The analytic sphere is exact
regardless — a genuine **quality argument for the analytic choice**, not a port bug, and it bounds the
useful tessellation (past N≈2 the sliver error overtakes the shrinking geometric faceting error).

**Hypothesis corrected by the multi-asset test.** I expected the most *anisotropic* asset (tornado) to
show the worst N=3 reversal; the data says otherwise — the **cloud** has by far the largest reversal
(3170 vs 475 vs 185 px), despite being the roundest. So the reversal tracks **per-primitive screen
footprint / overlap depth** (the cloud's 652 large, heavily-overlapping Gaussians each cover many
pixels, so each sliver error hits more pixels), not anisotropy per se. This is exactly why the
single-asset (cloud-only) result was not enough to judge — running tornado + bunny changed the
conclusion.

## Memory — instancing makes tessellation memory-cheap

The GAS is **instanced**: one unit icosphere shared by all 652 instances via the IAS, so the geometry
memory is the single GAS (1.4 KB at N=0 → 41.5 KB at N=3) regardless of primitive count — and the IAS
(instance records) is geometry-independent and unchanged. So tessellation's *memory* cost is trivial
here; the real cost is traversal **perf** (the window axis). **This flips the naive expectation** and
sharpens the comparison: the large memory win of analytic-over-tessellated only materialises if the
reference tessellates *per-primitive* instead of instancing one icosphere (the open Mitsuba lookup).

## Remaining for G8

- **Perf axis (window-only):** frame time + equal-quality `k` per N, analytic vs icosphere — the
  `frame_ms`/`k` columns in `icosphere.csv` are blank pending the locked-clock window. Hypothesis:
  built-in HW sphere intersector beats triangle-BVH traversal; low-N icospheres may be faster but
  faceted.
- **Mitsuba lookup:** icosphere degree + instanced-vs-per-primitive tessellation in
  `~/jorge/mitsuba3` ellipsoids plugin (determines the memory-comparison framing).
- **Figure + Ch 6 reclassification:** moves the analytic-vs-tessellated row out of `tab:four-modes`'s
  (I) "infeasible to ablate" column into a measured (M) result.

## Reproduce

```bash
cmake -S . -B build-ico -DCMAKE_BUILD_TYPE=Release -DTHESIS_ICOSPHERE=ON -DTHESIS_ICOSPHERE_N=2
cmake --build build-ico --target test_runner -j
SG_ENV=white_constant build-ico/bin/Release/test_runner --scene cloud_asset_validation --spp 64
# diff vs the analytic render from the canonical build/ (default OFF)
tools/refs/.venv/bin/python tools/refs/exr_diff.py /tmp/analytic_cloud.exr test_results/cloud_asset_validation/0000.exr
```
