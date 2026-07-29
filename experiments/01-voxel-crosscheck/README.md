# 01 — Independent voxel-grid cross-check (thesis Fig 5.1, §5.2.1)

**Claim.** In absorption, this renderer agrees with an independent voxel-grid integrator
(Mitsuba `advol` on a rasterised density grid) except for a resolution-limited silhouette
band that shrinks as the grid refines: the band is the grid's own discretisation error.

**Run.**
```
# ours (deterministic): cloud absorption render
SG_PLY=assets/models/cloud/root.primitives_pyr0.ply SG_RES=512 \
  build/bin/Release/test_runner --scene asset_validation --spp 16 --sigma-multiplier 10
# reference grids at increasing resolution (128/200/300/400/600), then compare:
../mitsuba-reference/README.md  # advol grid rendering + rasterisation
scripts/plots/voxel_gt_figure.py
```

**Expected.** Band RMSE falls with grid resolution (e.g. 200^3: 0.0627 -> 300^3: 0.0613
-> refined grids converge toward the analytic render); mean difference ~2e-5.
Runtime: minutes `[gpu]`; grid renders tens of minutes `[mitsuba]`.
