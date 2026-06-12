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

## Perf axis (measured 2026-06-11, 350 W + clock lock) — HYPOTHESIS REFUTED: the icosphere is FASTER

Cloud scattering (`cloud_asset_scattering`, meadow, `SG_CAM=0`, 128 spp, 900×600), both arms at the
shared production caps 128/128 (0 overflows both — fair). Fully interleaved
(analytic→N0→N1→N2→N3 per round, 5 rounds), prebuilt exe+optixir pairs swapped in place. Median of 5,
within-arm spread ≤1.5 %:

| arm | median frame time | ratio vs analytic |
|---|---|---|
| analytic (built-in sphere) | 15.52 s | 1.00× |
| icosphere N=0 (20 tris) | 9.80 s | **0.63×** |
| icosphere N=1 (80) | 11.22 s | **0.72×** |
| icosphere N=2 (320) | 11.99 s | **0.77×** |
| icosphere N=3 (1280) | 13.23 s | **0.85×** |

The spec's hypothesis ("analytic dominates — exact *and* faster") is **refuted on the perf half**:
the tessellated icosphere is faster at *every* N, and the analytic sphere pays **1.17–1.58×** for its
exactness. Cost grows monotonically with N, as expected. Likely mechanism: on RTX hardware, triangle
intersection runs on the dedicated RT-core hardware path, while the "built-in" sphere primitive
executes as a software intersection module on the traversal path — so the reference's tessellated
choice has a genuine performance rationale on this hardware class.

**Net G8 story — a real accuracy↔perf frontier, not a dominance result:** analytic = exact, at
1.17–1.58× frame time; tessellated = faster but biased, with the accuracy sweet spot at N≈1–2
(RMSE ~1e-3–1e-2, before the N=3 sliver reversal) and the perf sweet spot at N=0. Our analytic choice
is the *correctness*-optimal one, and its cost is now quantified.

**Operating-point caveat:** measured at 350 W with `-lgc 1800,1800` + `-lmc 9751,9751`, but the SM
clock thermally settled at **median 1605 MHz** (p5 1560, min 1365, max 1800 — the lock caps boost, it
does not prevent thermal pull-down). Within-arm jitter ≤1.5 % and full interleaving make the *ratios*
robust; absolute times are "at ~1.6 GHz". Equal-quality `k` is deliberately not used between arms —
the two converge to *different* images (faceting), so fixed-spp frame time is the clean
geometry-cost metric (`k` columns stay blank).

## Scattering-path gate (2026-06-12) — PASS

N=2 vs analytic, cloud scattering on the meadow (SG_CAM=0), 1024 spp both arms: converged means agree
to **+2.34e-4 on a 0.321 mean (0.073 %)** — even tighter than absorption's 0.16 %, sign flipped by the
scattering physics (smaller shell ⇒ less in-scatter here). Per-pixel mean|Δ| 2.8e-2 is decorrelated MC
noise between the arms, not bias. This exercises the full pipeline through triangles — argmin scatter
sampling, NEE/MIS shadow rays (TRANSMITTANCE-mode anyhit with the front-face filter) — so the port is
correct in scattering, not just absorption. Frame times en passant: analytic 196.5 s vs N=2 97.2 s at
1024 spp (the 2× here is *not* a clean ratio — arms ran sequentially, not interleaved; the interleaved
table above is the citable number).

## Reference-side shells (DSYG paper + shipped code) — lookup RESOLVED

- **Paper (DSYG §6.2 + Fig. 12):** custom ray-ellipsoid IS abandoned as "significantly less performant
  than a hardware-accelerated ray-triangle intersection test"; tessellated shells **duplicated per
  primitive** (instancing explicitly rejected: "reintroduces the problem of axis-aligned bounding boxes
  … in the instances BVH"); average **×4.96** speedup; shells tested: boxes 12△, icospheres **80△
  (=N1) / 320△ (=N2)**, UV spheres 42△/100△ — **best = 320△ icosphere**, never finer (consistent with
  our N=3 reversal).
- **Shipped code:** `mitsuba3/src/shapes/ellipsoidsmesh.cpp` **defaults to `ico_sphere` = 20△ (our
  N=0)**; the analytic-IS plugin (`ellipsoids.cpp`) exists separately.
- **Our own reference harness** (`tools/refs/render_*_via_prb.py`): defaults to
  **`ellipsoidsmesh` + `uv_sphere` (72△)** unless `SG_SHAPE=ellipsoids`. So the Mitsuba GT renders we
  gate against carry a (small) reference-side shell-faceting of their own. **Cheap follow-up:** re-run
  one strict energy gate with `SG_SHAPE=ellipsoids` (analytic reference shell) and see whether
  agreement tightens — quantifies the reference-side shell bias with zero new code.
- **Memory framing settled:** they duplicate per primitive (no instancing) → tessellation costs them
  O(N·tris); our instanced single-GAS keeps any shell at one copy (1.4–41.5 KB total). Their stated
  reason (instance-AABB quality) is a perf concern our A/B partially refutes at the 652-prim scale —
  our *instanced* icosphere still beat our *instanced* analytic sphere; instanced-vs-duplicated
  tessellation remains unmeasured (would need a duplicated-geometry GAS build).

## Remaining for G8
- ~~Mitsuba lookup~~ RESOLVED above.
- ~~Figure + Ch 6 reclassification~~ DONE 2026-06-12: `sec:icosphere` + `tab:icosphere` added to Ch 6,
  `tab:four-modes` (I) row emptied, `sec:reasoned` rewritten, Ch 4 hardware-intersector claim corrected
  (built-in sphere is *not* RT-core-accelerated). Optional: a dedicated frontier *figure* (RMSE vs
  frame time) if Ch 6 wants a visual; the table carries the data.
- (new, cheap) `SG_SHAPE=ellipsoids` reference-side re-gate, per above.
- (optional) duplicated-vs-instanced tessellation arm, only if the thesis wants to fully reproduce
  DSYG's geometry regime.

## Reproduce

```bash
cmake -S . -B build-ico -DCMAKE_BUILD_TYPE=Release -DTHESIS_ICOSPHERE=ON -DTHESIS_ICOSPHERE_N=2
cmake --build build-ico --target test_runner -j
SG_ENV=white_constant build-ico/bin/Release/test_runner --scene cloud_asset_validation --spp 64
# diff vs the analytic render from the canonical build/ (default OFF)
tools/refs/.venv/bin/python tools/refs/exr_diff.py /tmp/analytic_cloud.exr test_results/cloud_asset_validation/0000.exr
```
