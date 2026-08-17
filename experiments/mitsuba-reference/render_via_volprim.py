"""
Render the cloud PLY through Jorge's volprim_prb integrator (his reference renderer
for Gaussian primitives). Used to decompose the gap against refs_pyr0/:

    total_gap = renderer_math_gap + structural_gap
              = |our DSYG - volprim_prb on same PLY|
              + |volprim_prb on PLY - refs_pyr0/|

If volprim_prb at some sigma matches refs_pyr0/, the reference IS this PLY and our
job is just to match volprim_prb's renderer math. If no sigma matches, the reference
is not the PLY, period (Jorge's voxel-based claim is then the only explanation).

Outputs go to /tmp/volprim_sigma{N}/0000.exr for easy diffing.
"""

import argparse
import os
import sys
from pathlib import Path

import mitsuba as mi

mi.set_variant("cuda_ad_rgb")

# Volprim was written against an older Mitsuba API (put_parameter); Mitsuba 3.8 renamed
# it to put_value. Add an alias on the callback base class so volprim's traverse() works.
if hasattr(mi, "TraversalCallback") and not hasattr(mi.TraversalCallback, "put_parameter"):
    # Mitsuba 3.8's SceneTraversal.put_value requires (name, ptr, flags, cpptype);
    # the old put_parameter only passed three args. Default cpptype to None.
    def _put_parameter_compat(self, name, value, flags):
        return self.put_value(name, value, flags, None)
    mi.TraversalCallback.put_parameter = _put_parameter_compat

import volprim.integrators.volprim_prb  # noqa: F401  (registers integrator)

REPO_ROOT = Path(__file__).resolve().parents[2]
CLOUD_DIR = REPO_ROOT / "assets/models/cloud"


def render_one(sigmat_scale: float, spp: int, output_dir: Path,
               albedo: float = 0.0, cam_name: str = "cam_0000",
               analog: bool = False, seed: int = 0) -> Path:
    """Render the cloud PLY at given sigmat_scale through volprim_prb.

    analog=True runs use_nee=False (the analog estimator, unbiased in every
    revision). The default NEE estimator deterministically scales all direct
    environment radiance by 1/(1+(4pi)^-2) ~ 0.99371: at depth 0 the escape
    contribution is MIS power-heuristic weighted against the emitter pdf,
    which the (depth != 0) mask fails to zero (masked pdf_direction still
    returns 1/4pi for a constant emitter). Verified 2026-07-30 on an
    empty-view probe: NEE arm 0.9937074 = 1/(1+(4pi)^-2), analog arm 1.0.
    """
    sys.path.insert(0, str(CLOUD_DIR))
    # Re-import every call so we get a fresh module if cwd shifts
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
    if analog:
        scene_dict["integrator"] = dict(scene_dict["integrator"], use_nee=False)

    cam_cfg = dict(cloud_scene.SENSORS[cam_name])
    cam_cfg.pop("resources", None)
    scene_dict[cam_name] = cam_cfg

    scene = mi.load_dict(scene_dict)

    # Apply sigmat scaling and albedo override (matches refs_pyr0's absorber convention)
    params = mi.traverse(scene)
    params["primitives_pyr0.sigma_t"] = params["primitives_pyr0.sigma_t"] * sigmat_scale
    import numpy as np
    n_prims = len(params["primitives_pyr0.sigma_t"])
    params["primitives_pyr0.albedo"] = np.full(n_prims * 3, albedo, dtype=np.float32)
    params.update()

    output_dir.mkdir(parents=True, exist_ok=True)
    out_path = output_dir / (f"seed{seed}.exr" if seed else "0000.exr")

    img = mi.render(scene, sensor=scene.sensors()[0], spp=spp, seed=seed)
    mi.util.write_bitmap(str(out_path), img)
    return out_path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--sigma", type=float, action="append", required=True,
                   help="sigmat_scale to render. Pass multiple times for a sweep.")
    p.add_argument("--spp", type=int, default=64)
    p.add_argument("--albedo", type=float, default=0.0,
                   help="Override albedo (0 = absorber, matches refs_pyr0)")
    p.add_argument("--analog", action="store_true",
                   help="use_nee=False (analog estimator; see render_one docstring)")
    p.add_argument("--seed", type=int, action="append", default=None,
                   help="Sampler seed(s); repeat for multi-seed averaging. Default [0].")
    p.add_argument("--output-root", type=Path, default=Path("/tmp"),
                   help="Each sigma writes to <root>/volprim_sigma{N}/0000.exr")
    args = p.parse_args()

    for s in args.sigma:
        out_dir = args.output_root / f"volprim_sigma{s}"
        for seed in (args.seed or [0]):
            path = render_one(s, args.spp, out_dir, albedo=args.albedo,
                              analog=args.analog, seed=seed)
            print(f"sigma={s} seed={seed} -> {path}")


if __name__ == "__main__":
    main()
