# Voxel-grid ground truth — capability + analysis (2026-06-13)

**Verdict: CAPABLE, proven end-to-end on the real cloud.** An independent dense-grid path tracer
reproduces our cloud. This is a genuinely stronger validation than the existing Mitsuba-volprim
cross-check, and a viable thesis addition — with caveats (below) before it's *quantitative*.

## What was proven
- The render machinery works in our env (`tools/refs/with_jorge_mitsuba.sh` + `.venv`, mitsuba 3.6.4):
  Mitsuba `heterogeneous` medium, `sigma_t` a `gridvolume`, `volpath` integrator (smoke test:
  `tools/refs/voxel_smoke.py` → `voxel_smoke.png`, a clean absorbing blob).
- On the **real cloud**: `tools/refs/voxel_cloud.py` voxelizes the 652 Gaussians into a dense sigma_t
  grid **in OUR renderer's convention** (`sigma_t(x)=Σ (7.5·sigma_t_i)·(2π)^{-3/2}∏(1/s_ij)·exp(-½‖S⁻¹Rᵀ(x-μ)‖²)`,
  matching `primitive.h`), then path-traces it. Result (`voxel_cloud.png`, `voxel_vs_ours.png`) is the
  same lumpy cloud as ours/volprim.

## Why this is valuable (the independent-GT argument)
The Ch5 validation compares ours vs Mitsuba-**volprim** — but BOTH are analytic-Gaussian renderers; they
share the per-primitive-Gaussian integration approach and could in principle share a systematic
convention assumption (normalisation, kernel form). The voxel render **breaks that shared assumption**:
it integrates a *discretised density field* by brute-force delta-tracking, with **no Gaussian math at
all** and no shared code with either renderer. Agreement therefore validates our **density convention +
optical-depth integration** against a fundamentally different method — a third, independent leg.

## What it does NOT prove
It is a *renderer/convention* GT, not a *representation-fidelity* GT. It does NOT validate the Gaussian
**fit** against the ORIGINAL source volume — the Disney WDAS cloud (`args.json`:
`resources/vdb/wdas_cloud_eighth.npy`, sigmat_scale 7.5). That source is **not on the machine**; only
`assets/models/cloud/pyramid_level_0.npy` is present, a (250,170,307) grid in a *different* convention
(max 0.88, not our optical-depth scale) — so it can't be rendered directly as a matched GT. Validating
the fit would need the original VDB.

## Caveats found / what a QUANTITATIVE GT needs
1. **Dynamic range / majorant — the big one.** Peak sigma_t ≈ **7323** (sharp sub-voxel Gaussians). Grid
   delta-tracking cost scales with the majorant → a full-density render is brutally slow (a 128³/128spp
   render did not finish in 10 min). For this *shape* check I clamped sigma_t to 80 — the dense cores are
   opaque either way, so the absorption image is unchanged, but a quantitative energy GT must lift the
   clamp (and/or use a majorant/super-voxel grid). **This high dynamic range is itself a result:** the
   analytic Gaussian renderer handles it in closed form; the grid method pays dearly for it.
2. **Resolution.** Gaussian scales span 0.02–0.13; the smallest are sub-voxel at 128³ → crinkled edges,
   lost wisps, slight under-absorption. A quantitative GT needs ~300–600³ (memory + much slower render).
   Cloud-first: the bunny's 25600 tiny Gaussians would need an enormous grid.
3. **Camera registration.** Used cam_0000's `look_at` (assets/models/cloud/__init__.py) + a hand-tuned
   orthographic scale (=1.0 framed it). A pixel-registered diff needs the exact volprim ortho-extent
   convention, or loading the __init__.py scene and swapping only the medium.
4. **Jorge's pre-baked `refs_voxel_self/` are NOT usable** — rendered with generic rotating cameras
   (`opt_utils.prepare_cameras`), not cam_0000, so they're mis-framed dark blobs (mean 0.13 vs our 0.42).

## Cross-check sanity (existing data)
Our absorption render and Mitsuba-volprim's `refs_prb_absorption` match **exactly** (both mean 0.4163),
so the Gaussian-vs-Gaussian leg is already tight; the voxel leg adds the independent-integrator leg.

## Files (investigation, uncommitted)
`tools/refs/voxel_smoke.py`, `tools/refs/voxel_cloud.py` (args: VOX SPP ORTHO_SCALE; grid cached at
/tmp/cloud_grid_<VOX>.npy), `results/campaign/voxel_{smoke,cloud,compare,vs_ours}.png`.

## Recommendation
Worth pursuing as a Ch5/Ch7 addition (independent GT for the density model). Effort: moderate —
resolution + majorant handling + camera registration, cloud-first, absorption-first (deterministic).
Decision for Kacper: pursue the quantitative version, or cite this qualitative confirmation.

## UPDATE: matched-camera lit comparison (2026-06-13)
Fixed both issues with the first comparison (black silhouette + wrong camera):
- Lit, not black: scattering (albedo 0.9, HG g=0.85, sigmat 7.5) under the meadow envmap (roty 90)
  instead of pure absorption (tools/refs/voxel_cloud_scene.py).
- Exact camera: loaded the real cloud scene (assets/models/cloud/__init__.py -> cam_0000) and swapped
  ONLY the medium (Gaussian ellipsoids -> heterogeneous/gridvolume), so the camera matches our g1/refs
  exactly. Also fixed a gridvolume axis-order bug (Mitsuba indexes [Z,Y,X]; our grid was [X,Y,Z] ->
  transpose) that had rotated the volume.
Result results/campaign/gaussians_vs_voxgrid_lit.png: same cloud, camera, lighting; mean 0.338 (voxel)
vs 0.322 (ours), ~5%. Residual = 32-spp scatter noise + @80 clamp + 200^3 under-resolution.

## RIGOROUS GT EXPERIMENT (2026-06-14, branch feature/voxel-gt)
Goal: a *valid* (unclamped-equivalent) voxel GT for absorption AND scattering. Pipeline:
voxelize our Gaussians in our convention (`voxel_build.py`, σ_t = integrated mass M ×7.5 ×(2π)^-3/2∏(1/s)
— matches `ply.cpp:109` + `primitive.h`, **mass-conserving, verified no normalization factor**), then
render through Mitsuba's independent `heterogeneous`/`gridvolume`/`prbvolpath` from the EXACT cloud scene
(`voxel_gt_render.py` loads `__init__.py` → cam_0000, swaps only the medium; gridvolume axis transpose
[X,Y,Z]→[Z,Y,X]).

### Tractability — SOLVED via clamp-convergence
Stock Mitsuba 3.6 uses the gridvolume's global max as the delta-tracking majorant (peak σ_t≈8900 here)
→ unclamped ratio-tracking is slow (32-spp absorption did not finish in minutes). Fix: **clamp the
majorant and prove it harmless** by raising it until the image stops changing. Absorption at 200³:
clamp 80/250/800/2000 give an IDENTICAL image (mean 0.3983, RMSE 0.0627; clamp 2000 only confirms 80) —
the dense cores are opaque regardless. So clamp@250 is a fast (~17 s) **valid unclamped-equivalent**.

### Absorption GT — VALID independent cross-check, bulk-exact, edge-resolution-limited
Resolution sweep (cam_0000, clamp 250, 32 spp) vs our analytic absorption render (`ico_fig/analytic`,
mean 0.4163). `voxgt_abs_convergence.{csv,png}`:

| res | mean | RMSE vs ours |
|---|---|---|
| 128 | 0.3942 | 0.0724 |
| 200 | 0.3983 | 0.0627 |
| 300 | 0.4000 | 0.0590 |
| 400 | 0.4007 | 0.0576 |
| 600 | 0.4011 | 0.0567 |

Monotone convergence toward ours. **The diff (`voxgt_abs_diff.png`) is a thin band on the silhouette
ONLY** — core + background match EXACTLY (median pixel diff = 0.0000; 5% of error in the 54%-of-pixels
core). The residual is the grid blurring the sharp Gaussian boundary (spreads density outward → slightly
darker edges → mean plateaus ~0.401, 3.6% under ours). It shrinks with res but slowly (σ_min≈0.02 needs
huge grids for crisp edges). More spp does NOT help (0.0627→0.0613 at 128 spp) — it's resolution, not
noise. Supersampling (SS=4) did not help (trilinear reconstruction re-spreads). **Verdict: an independent
grid integrator confirms our renderer everywhere it can resolve the field; the only disagreement is the
sub-voxel-sharp boundary the grid cannot represent — which argues FOR the analytic primitive.**

### Scattering GT — mean agrees (~6%), but firefly-limited per-pixel
200³, meadow, albedo 0.9, HG 0.85, 32 spp, vs our g1 mean (0.3214): clamp 80 mean 0.3379 (RMSE 0.18);
clamp 400 mean 0.3408 (**RMSE 1.13**). The mean is stable (~0.34, ~6% above ours) but RMSE EXPLODES as
the clamp rises — the dense cores produce extreme scattering **fireflies** (the same high-variance
problem as G1's analog GT, k≈3899). A clean *per-pixel* scattering GT therefore needs massive spp +
firefly handling; the qualitative lit match is `gaussians_vs_voxgrid_lit.png`. **Absorption is the clean
anchor; scattering confirms in the mean only** — exactly the brainstorm's "absorption-floor,
scattering-stretch" call.

### Bottom line
The voxel GT is REAL, TRACTABLE, and a valid independent third validation leg: **absorption** confirms
our renderer to a bulk-exact, monotonically-converging, edge-resolution-limited agreement; **scattering**
agrees in the mean (~6%) but is firefly-limited per-pixel. Honest framing for the thesis: "an independent
dense-grid path tracer agrees with the renderer wherever the grid resolves the field." (a) renderer-check
done; (b) the *representation*-fidelity GT vs the original Disney VDB still needs that source from Jorge.
Scripts/figures committed on `feature/voxel-gt`.
