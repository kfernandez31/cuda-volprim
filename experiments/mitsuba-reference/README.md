# tools/refs — self-generated reference renders

Renders ground-truth voxel-grid references for asset validation, and diffs the
thesis renderer's output against them. Standalone Python tool; not part of the
CMake build.

## Why

The original `assets/models/cloud/refs_pyr0/` references were produced by an unknown
script (Jorge says a voxel-grid renderer; the asset's `__init__.py` config
points at a Gaussian-ellipsoid path tracer). To stop calibrating constants
against a black-box reference, this tool regenerates references locally with
Mitsuba's stock `prbvolpath` + `gridvolume` plugins so we control the provenance.

## Setup (one-time)

```bash
python3 -m venv tools/refs/.venv
tools/refs/.venv/bin/pip install -r tools/refs/requirements.txt
```

Requires CUDA + an NVIDIA GPU (Mitsuba uses the `cuda_ad_rgb` variant).

## Workflow

```bash
# 1. Generate voxel references (re-run only when the voxel grid changes)
tools/refs/.venv/bin/python tools/refs/render_voxel_reference.py --asset cloud --spp 256

# 2. Render the thesis DSYG output
./build/bin/Release/test_runner --scene cloud_asset_validation --sigma-multiplier 7.5

# 3. Compare
tools/refs/.venv/bin/python tools/refs/compare_renders.py \
    assets/models/cloud/refs_voxel_self/ \
    test_results/cloud_asset_validation/
```

`compare_renders.py` reports per-frame RMSE / MAE / silhouette IoU / phase-shift,
and an aggregate row. Pass `--out diffs/` to also write per-frame `|ref - test|`
EXRs.

## Adding a new asset

When a new voxel grid lands locally, add an entry to `ASSETS` in
`render_voxel_reference.py`:

```python
"bunny": AssetConfig(
    voxel_grid=REPO_ROOT / "assets/models/bunny/<grid>.npy",
    ply=REPO_ROOT / "assets/models/bunny/<primitives>.ply",
    scene_module=REPO_ROOT / "assets/models/bunny/__init__.py",
    output_dir=REPO_ROOT / "assets/models/bunny/refs_voxel_self",
    density_scaler=10.0,       # from Jorge's density-scaler table
    albedo=0.0,
),
```

The voxel grid bbox is auto-fit from the PLY (`center ± 3σ` over all primitives,
matching `GAUSSIAN_EXTENT_F` in `include/thesis/common/utils/math.h`).
