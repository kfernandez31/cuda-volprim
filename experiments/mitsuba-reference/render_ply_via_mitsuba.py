"""
Render the cloud PLY through Mitsuba's volprim_tomography integrator at
cloud/__init__.py's cam_0000. Produces the reference EXR our CUDA renderer
should match pixel-for-pixel (modulo MC noise) when given the same PLY +
same camera + same opacity multiplier + albedo=0.

Apples-to-apples: same primitive representation (Gaussian ellipsoids from
the same PLY), same physics (absorption only at albedo=0), same camera.
Any disagreement is a bug in our CUDA renderer's kernel.

Run via:
    experiments/mitsuba-reference/with_jorge_mitsuba.sh \
        experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/render_ply_via_mitsuba.py

Requires Jorge's custom Mitsuba build (has RayFlags.BackfaceCulling that
volprim's integrators use; removed in upstream Mitsuba >=3.5.2).
"""
import os
import sys

import numpy as np
import mitsuba as mi

mi.set_variant("cuda_ad_rgb")
import drjit as dr
from mitsuba import ScalarTransform4f as T

# Register volprim_tomography
import volprim.integrators.volprim_tomography  # noqa: F401

# Load cloud/__init__.py's SENSORS dict for the camera.
# Use the local copy under assets/ if it exists; otherwise fall back to ~/jorge/cloud.
THESIS_ROOT = "/home/kacper/thesis"
LOCAL_CLOUD_INIT = os.path.join(THESIS_ROOT, "assets/models/cloud")
JORGE_CLOUD_INIT = "/home/kacper/jorge/cloud"
CLOUD_INIT_DIR = LOCAL_CLOUD_INIT if os.path.exists(os.path.join(LOCAL_CLOUD_INIT, "__init__.py")) else JORGE_CLOUD_INIT
sys.path.insert(0, CLOUD_INIT_DIR)
import __init__ as cloud_scene

PLY_PATH = os.path.join(THESIS_ROOT, "assets/models/cloud/root.primitives_pyr0.ply")
OUT_DIR = os.path.join(THESIS_ROOT, "test_results/ply_via_mitsuba")
os.makedirs(OUT_DIR, exist_ok=True)

# Parameters — keep aligned with our CUDA renderer's defaults so the comparison is fair.
# sigma_multiplier in our CUDA renderer corresponds to the linear multiplier
# applied to per-primitive sigma_t after PLY load.
SIGMA_MULTIPLIER = 7.5   # Jorge's "cloud 7.5" / args.json sigmat_scale
SPP = 1024
CAM_NAME = "cam_0000"

# Build scene: integrator + the cloud's ellipsoidsmesh primitives + constant env.
cam_cfg = cloud_scene.SENSORS[CAM_NAME].copy()
cam_cfg.pop("resources", None)

scene_dict = {
    "type": "scene",
    "integrator": {
        "type": "volprim_tomography",
        "max_depth": 32,
        # kernel_type defaults to 'gaussian' in volprim; explicit for documentation.
        "kernel_type": "gaussian",
    },
    "primitives_pyr0": {
        "type": "ellipsoidsmesh",
        "shell": "uv_sphere",   # smooth shell — default is faceted ("hexagonal crystals")
        "filename": PLY_PATH,
        "extent": 3.0,
    },
    "environment": {
        "type": "constant",
        "radiance": {"type": "uniform", "value": 1.0},
    },
    CAM_NAME: cam_cfg,
}

scene = mi.load_dict(scene_dict)

# Apply sigma_multiplier and force albedo=0 (pure absorber).
params = mi.traverse(scene)
sigma_t = params["primitives_pyr0.sigma_t"]
st_np = np.array(sigma_t)
print(f"Loaded {len(sigma_t)} primitives. sigma_t pre-multiplier: "
      f"min={st_np.min():.4g} max={st_np.max():.4g} mean={st_np.mean():.4g}")
params["primitives_pyr0.sigma_t"] = sigma_t * SIGMA_MULTIPLIER

n = len(params["primitives_pyr0.sigma_t"])
params["primitives_pyr0.albedo"] = np.zeros(n * 3, dtype=np.float32)
params.update()

print(f"Rendering {CAM_NAME} at spp={SPP}, sigma_multiplier={SIGMA_MULTIPLIER}, albedo=0 ...")
img = mi.render(scene, sensor=scene.sensors()[0], spp=SPP, seed=0)
dr.sync_thread()

out_path = os.path.join(OUT_DIR, f"{CAM_NAME}_volprim_tomography_spp{SPP}.exr")
mi.Bitmap(img).write(out_path)
arr = np.array(img).astype(np.float32)
print(f"wrote {out_path}  size={os.path.getsize(out_path)}  "
      f"mean={arr.mean():.4f}  max={arr.max():.4f}")
