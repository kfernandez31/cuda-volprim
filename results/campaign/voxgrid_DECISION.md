# Voxel-grid cross-check — EXCLUDED from the thesis (decision 2026-06-15)

**Decision (Kacper):** do NOT include any voxel-grid comparison, and no mention of AdVol, in the thesis.
This file records the full investigation so the work isn't lost; the thesis validation rests instead on
the analytic closed-form ground truth, the Mitsuba-volprim differential comparison, and the
energy-conserving furnace invariant.

## What the voxel-grid idea was
Our renderer and the Mitsuba reference both integrate the medium as a sum of analytic Gaussians, so a
flaw in that shared math could hide in both. A dense voxel grid rendered by delta-tracking shares no
code and no Gaussian math, so agreement would be a stronger, independent witness.

## What we found (full detail in `voxel_gt.md` and `advol_gt.md`)
- **Absorption: clean and conclusive.** The grid render matched ours bulk-exact (median pixel diff 0),
  the only residual a silhouette band that shrinks monotonically with grid resolution (RMSE
  0.072→0.057 from 128³→600³). Discretisation, not noise. This leg was solid.
- **Scattering: a fundamental wall.** The cloud's density spans 0→~8900 (sharp sub-voxel cores). That
  forces an unavoidable trilemma for any grid tracker:
  - **Local majorants (AdVol supervoxels):** low-variance/clean image BUT a per-block approximation →
    rectangular "glitch" artifacts + a ~3% setting-dependent bias (mean shifts with supervoxel block
    size: 0.320 @sv8 → 0.329 @sv2). A 1-voxel-halo majorant fix removed the *gross* blocks but a
    residual remained; higher grid resolution (400³) shrank the blocks but the bias was unchanged
    (~+2.6%), confirming it is block-approximation-tied, not resolution-tied.
  - **Global majorant (unbiased, no blocks):** correct and glitch-free by construction BUT the majorant
    (~8900) makes delta-tracking mostly null collisions → extreme fireflies + intractably slow
    (unclamped 128 spp never finished; clamp-convergence didn't plateau at feasible spp:
    clamp 250→mean 0.338 clean, clamp 800→0.332 fireflies max 1034, clamp 2000→0.314 in 32 min/seed).
  You cannot get clean **and** unbiased **and** tractable from a grid for this high-dynamic-range medium.
- **Why (and why it's a point FOR the representation):** the grid struggles precisely because it lacks
  the variance reduction (MIS / environment importance sampling + analytic transmittance) our renderer
  has built in. The analytic Gaussian renderer is better-conditioned for this than a brute-force grid.

## Why excluded rather than reported as a soft bracket
The absorption leg alone was clean, but the scattering leg could only be presented as a ~3–5% bracket
with caveats (block artifacts / firefly-limited / supervoxel-tracker softness). The cleaner, simpler,
fully-defensible story is to validate scattering with the furnace invariant (independent physics) +
Mitsuba-volprim differential + analytic GT, and not introduce the voxel-grid machinery and its caveats.

## Artifacts kept (NOT referenced by the thesis)
- Scripts: `tools/refs/voxel_build.py`, `voxel_gt_render.py`, `voxel_gt_advol.py`,
  `scripts/plots/voxel_scatter_gt.py` (+ the halo-majorant monkeypatch in `voxel_gt_advol.py`).
- Records: `voxel_gt.md` (absorption + early scattering), `advol_gt.md` (AdVol + halo fix), this file.
- Cached grids: `results/campaign/voxgrids/`; renders: `results/campaign/advol_seeds/` (gitignored).
- AdVol itself lives at `~/jorge/advol` (Jorge's DSYG grid baseline; not on the thesis path).

## If ever revisited
The only routes to a pristine per-pixel scattering grid GT are (a) days-long unclamped global renders at
350 W, or (b) porting AdVol's local-majorant machinery with a correct halo — both high-effort, low-marginal-
value given the existing three-legged validation. The representation-fidelity GT (our fit vs the ORIGINAL
Disney WDAS volume) remains separately gated on the source `wdas_cloud_eighth.npy` (not on disk).
