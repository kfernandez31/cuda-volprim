"""
Render ground-truth voxel references for asset validation.

Uses Mitsuba's stock prbvolpath + gridvolume to render a voxel-grid version of
the asset, from each camera defined in the asset's scene module. Outputs EXRs
that the thesis DSYG renderer can be compared against.

Run from project root:
    tools/refs/.venv/bin/python tools/refs/render_voxel_reference.py --asset cloud
"""

from __future__ import annotations

import argparse
import importlib.util
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict

import numpy as np
import mitsuba as mi
from plyfile import PlyData

mi.set_variant("cuda_ad_rgb")


# Each entry is a single asset we can render. Add new ones as voxel grids land
# locally (bunny, smoke, tornado, ...). Density scaler values come from Jorge's
# table; cloud=7.5 also matches assets/cloud/args.json:sigmat_scale.
@dataclass(frozen=True)
class AssetConfig:
    voxel_grid: Path           # .npy density grid
    ply: Path                  # Gaussian PLY (used only to derive bbox)
    scene_module: Path         # __init__.py defining SENSORS dict
    output_dir: Path
    density_scaler: float
    albedo: float


REPO_ROOT = Path(__file__).resolve().parents[2]

ASSETS: Dict[str, AssetConfig] = {
    "cloud": AssetConfig(
        voxel_grid=REPO_ROOT / "assets/cloud/pyramid_level_0.npy",
        ply=REPO_ROOT / "assets/cloud/root.primitives_pyr0.ply",
        scene_module=REPO_ROOT / "assets/cloud/__init__.py",
        output_dir=REPO_ROOT / "assets/cloud/refs_voxel_self",
        density_scaler=7.5,
        albedo=0.0,
    ),
}


def gaussian_bbox_from_ply(ply_path: Path, extent_sigma: float = 3.0) -> tuple[np.ndarray, np.ndarray]:
    """Return (min_corner, max_corner) of the Gaussian cloud's world-space bbox.

    bbox = union over primitives of (center - extent_sigma * scale, center + extent_sigma * scale),
    where scale is the un-rotated per-axis std-dev. This matches the BVH bound the thesis
    renderer uses (GAUSSIAN_EXTENT_F = 3.0 in include/thesis/common/utils/math.h).

    Ignoring per-primitive rotation here is a conservative over-approximation — the rotated
    ellipsoid lies inside the axis-aligned box of side 2*extent_sigma*max(|scale|). For volume
    placement purposes that's fine: we just need the voxel grid to cover everywhere the
    Gaussians live.
    """
    ply = PlyData.read(str(ply_path))
    v = ply["vertex"].data
    centers = np.stack([v["x"], v["y"], v["z"]], axis=-1)
    # PLY stores log-scale (consistent with src/thesis/host/utils/io/ply.cpp:102).
    log_scales = np.stack([v["scale_0"], v["scale_1"], v["scale_2"]], axis=-1)
    scales = np.exp(log_scales)
    half_extent = extent_sigma * scales
    return (centers - half_extent).min(axis=0), (centers + half_extent).max(axis=0)


def to_world_mapping(min_corner: np.ndarray, max_corner: np.ndarray,
                     local_min: float) -> mi.ScalarTransform4f:
    """Linear transform mapping [local_min, 1]^3 (per-axis) to [min, max] in world space.

    Mitsuba uses two different local conventions in the same scene graph:
      - `cube` shape:  local coords span [-1, 1]^3   -> pass local_min=-1
      - `gridvolume`:  local coords span [ 0, 1]^3   -> pass local_min= 0

    Passing the same to_world to both misplaces them by a 2x scale + offset, which
    shows up as huge phase shifts on the rendered silhouette.
    """
    local_extent = 1.0 - local_min                  # 2.0 for cube, 1.0 for gridvolume
    s = (max_corner - min_corner) / local_extent
    t = min_corner - s * local_min                  # picks the right offset so local_min -> min_corner
    m = np.eye(4, dtype=np.float64)
    m[0, 0], m[1, 1], m[2, 2] = s
    m[0, 3], m[1, 3], m[2, 3] = t
    return mi.ScalarTransform4f(m)


def load_scene_module(path: Path):
    """Load assets/cloud/__init__.py as a module so we can pull SENSORS, EMITTERS."""
    spec = importlib.util.spec_from_file_location("asset_scene", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def render_asset(cfg: AssetConfig, spp: int, max_depth: int) -> None:
    asset_dir = cfg.scene_module.parent
    sys.path.insert(0, str(asset_dir))
    scene_mod = load_scene_module(cfg.scene_module)

    # Voxel grid: (Z, Y, X) float64 -> (Z, Y, X, 1) float32 for Mitsuba's VolumeGrid.
    grid_np = np.load(str(cfg.voxel_grid)).astype(np.float32)
    if grid_np.ndim == 3:
        grid_np = grid_np[..., None]
    print(f"Voxel grid: shape={grid_np.shape}, range=[{grid_np.min():.4f}, {grid_np.max():.4f}]")
    volume_grid = mi.VolumeGrid(grid_np)

    min_corner, max_corner = gaussian_bbox_from_ply(cfg.ply)
    print(f"Gaussian bbox: min={min_corner}, max={max_corner}")
    cube_to_world = to_world_mapping(min_corner, max_corner, local_min=-1.0)
    grid_to_world = to_world_mapping(min_corner, max_corner, local_min=0.0)

    cfg.output_dir.mkdir(parents=True, exist_ok=True)

    base_scene = {
        "type": "scene",
        "integrator": {"type": "prbvolpath", "max_depth": max_depth},
        "object": {
            "type": "cube",
            "to_world": cube_to_world,    # cube spans local [-1, 1]^3 -> bbox in world
            "bsdf": {"type": "null"},
            "interior": {
                "type": "heterogeneous",
                "sigma_t": {
                    "type": "gridvolume",
                    "grid": volume_grid,
                    "to_world": grid_to_world,   # grid spans local [0, 1]^3 -> bbox in world
                },
                "albedo": cfg.albedo,
                "scale": cfg.density_scaler,
            },
        },
        "environment": scene_mod.EMITTERS["environment"],
    }

    camera_names = sorted(k for k in scene_mod.SENSORS if k.startswith("cam_"))
    print(f"Rendering {len(camera_names)} cameras at spp={spp}, max_depth={max_depth}")

    for i, cam_name in enumerate(camera_names):
        cam_cfg = dict(scene_mod.SENSORS[cam_name])
        cam_cfg.pop("resources", None)
        scene_dict = dict(base_scene)
        scene_dict[cam_name] = cam_cfg
        scene = mi.load_dict(scene_dict)

        cam_idx = int(cam_name.split("_")[1])
        out_path = cfg.output_dir / f"{cam_idx:04d}.exr"
        print(f"  [{i+1}/{len(camera_names)}] {cam_name} -> {out_path.name}")
        img = mi.render(scene, sensor=scene.sensors()[0], spp=spp)
        mi.util.write_bitmap(str(out_path), img)

    print(f"Done. Output: {cfg.output_dir}")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--asset", choices=sorted(ASSETS), required=True)
    p.add_argument("--spp", type=int, default=256)
    p.add_argument("--max-depth", type=int, default=32,
                   help="Path-tracing max depth. Match thesis renderer (consts::MAX_BOUNCES).")
    p.add_argument("--output-dir", type=Path, default=None,
                   help="Override asset's default output dir.")
    p.add_argument("--density-scaler", type=float, default=None,
                   help="Override the asset's density scaler (Jorge's table value).")
    args = p.parse_args()

    cfg = ASSETS[args.asset]
    overrides = {}
    if args.output_dir is not None:
        overrides["output_dir"] = args.output_dir
    if args.density_scaler is not None:
        overrides["density_scaler"] = args.density_scaler
    if overrides:
        cfg = AssetConfig(**{**cfg.__dict__, **overrides})
    print(f"density_scaler={cfg.density_scaler}, albedo={cfg.albedo}")

    render_asset(cfg, spp=args.spp, max_depth=args.max_depth)


if __name__ == "__main__":
    main()
