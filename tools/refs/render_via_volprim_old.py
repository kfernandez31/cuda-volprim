"""Same as render_via_volprim.py but for the older mitsuba 3.5 + drjit 0.4 venv.
Uses native API (no monkey-patches needed)."""

import argparse
import sys
from pathlib import Path

import mitsuba as mi

mi.set_variant("cuda_ad_rgb")
import volprim.integrators.volprim_prb  # noqa: F401

REPO_ROOT = Path(__file__).resolve().parents[2]
CLOUD_DIR = REPO_ROOT / "assets/models/cloud"


def render_one(sigmat_scale: float, spp: int, output_dir: Path,
               albedo: float = 0.0, cam_name: str = "cam_0000") -> Path:
    sys.path.insert(0, str(CLOUD_DIR))
    if "asset_scene" in sys.modules:
        del sys.modules["asset_scene"]
    import importlib.util
    spec = importlib.util.spec_from_file_location("asset_scene", CLOUD_DIR / "__init__.py")
    cloud_scene = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(cloud_scene)

    scene_dict = {"type": "scene"}
    scene_dict.update(cloud_scene.OBJECTS)
    scene_dict.update(cloud_scene.EMITTERS)
    scene_dict.pop("resources", None)
    scene_dict["primitives_pyr0"].pop("extent_adaptive_clamping", None)
    scene_dict["primitives_pyr0"]["filename"] = str(CLOUD_DIR / "data/root.primitives_pyr0.ply")

    cam_cfg = dict(cloud_scene.SENSORS[cam_name])
    cam_cfg.pop("resources", None)
    scene_dict[cam_name] = cam_cfg

    scene = mi.load_dict(scene_dict)

    params = mi.traverse(scene)
    params["primitives_pyr0.sigma_t"] = params["primitives_pyr0.sigma_t"] * sigmat_scale
    import numpy as np
    n_prims = len(params["primitives_pyr0.sigma_t"])
    params["primitives_pyr0.albedo"] = np.full(n_prims * 3, albedo, dtype=np.float32)
    params.update()

    output_dir.mkdir(parents=True, exist_ok=True)
    out_path = output_dir / "0000.exr"

    img = mi.render(scene, sensor=scene.sensors()[0], spp=spp)
    mi.util.write_bitmap(str(out_path), img)
    return out_path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--sigma", type=float, action="append", required=True)
    p.add_argument("--spp", type=int, default=64)
    p.add_argument("--albedo", type=float, default=0.0)
    p.add_argument("--output-root", type=Path, default=Path("/tmp"))
    args = p.parse_args()

    for s in args.sigma:
        out_dir = args.output_root / f"volprim_sigma{s}"
        path = render_one(s, args.spp, out_dir, albedo=args.albedo)
        print(f"sigma={s} -> {path}")


if __name__ == "__main__":
    main()
