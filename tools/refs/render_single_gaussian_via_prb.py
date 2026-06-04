"""
Render the same single-Gaussian validation scene through Mitsuba's
volprim_tomography, so we can compare it against the closed-form
single_gaussian_analytic.py (the same comparator used for our CUDA
renderer).

If Mitsuba matches H_analog and CUDA matches H_double (current finding),
the closed-form is correct and our CUDA renderer has a double-count bug.
If Mitsuba ALSO matches H_double, the analytic formula is wrong.

Run via:
    tools/refs/with_jorge_mitsuba.sh \
        tools/refs/.venv/bin/python tools/refs/render_single_gaussian_via_mitsuba.py
"""
import os
import numpy as np
import mitsuba as mi

mi.set_variant("cuda_ad_rgb")
import drjit as dr
from mitsuba import ScalarTransform4f as T

import volprim.integrators.volprim_prb  # noqa: F401

# These MUST match test/scenes/single_gaussian.cpp constants.
WIDTH = 256
HEIGHT = 256
ORTHO_HEIGHT = 6.0
CAMERA_DISTANCE = 5.0

import math

# Convention (post-fix, commit 241d47b): both renderers use sigma_t == total mass M
# directly, with NO (2π)^{3/2} bridge.
#   CUDA:    optical_thickness_ = sigma_multiplier;  tau(0) = M/(2π)
#   Mitsuba: tau = sigma_t · density_integral;  density_integral(0) = 1/(2π) for
#            scale=1, |w|=1  ⇒  tau(0) = sigma_t/(2π)
# So to match CUDA --sigma-multiplier S we feed Mitsuba sigma_t = S (NOT S·(2π)^{3/2}).
SIGMA_MULTIPLIER_CUDA = float(os.environ.get("SG_SIGMA", "4.0"))
SIGMA_T_MITSUBA = SIGMA_MULTIPLIER_CUDA
SPP = int(os.environ.get("SG_SPP", "1024"))

THESIS_ROOT = "/home/kacper/thesis"
OUT_DIR = os.path.join(THESIS_ROOT, "test_results/single_gauss")
os.makedirs(OUT_DIR, exist_ok=True)

# Build single ellipsoid programmatically via ellipsoidsmesh's tensor inputs.
# center=(0,0,0), scale=(1,1,1), identity quaternion, opacity = sigma_multiplier,
# albedo = 0 (pure absorber). extent=3.0 wraps the 3σ bbox.
N = 1
centers = mi.TensorXf(np.array([[0.0, 0.0, 0.0]], dtype=np.float32).ravel(),
                      shape=(N, 3))

# SG_TRANSFORMED=1 mirrors test/scenes/single_gaussian.cpp's transformed config
# EXACTLY: anisotropic scale (1.0, 0.5, 0.75) + forward (local->world) rotation
# 30 deg about +Z. Mitsuba quaternion order is (x, y, z, w); a +Z rotation by
# theta is (0, 0, sin(theta/2), cos(theta/2)).
if os.environ.get("SG_TRANSFORMED") == "1":
    _scale = [1.0, 0.5, 0.75]
    _half = math.radians(30.0) / 2.0
    _quat = [0.0, 0.0, math.sin(_half), math.cos(_half)]
else:
    _scale = [1.0, 1.0, 1.0]
    _quat = [0.0, 0.0, 0.0, 1.0]

scales = mi.TensorXf(np.array([_scale], dtype=np.float32).ravel(), shape=(N, 3))
# Mitsuba quaternion: (x, y, z, w)
quaternions = mi.TensorXf(np.array([_quat], dtype=np.float32).ravel(),
                          shape=(N, 4))
sigma_t = mi.TensorXf(np.array([[SIGMA_T_MITSUBA]], dtype=np.float32).ravel(),
                      shape=(N, 1))
# SG_ALBEDO drives the scattering campaign (mirrors test/scenes/single_gaussian.cpp).
# 0 = pure absorber; >0 turns on in-scattering. albedo=1 = furnace test.
ALBEDO = float(os.environ.get("SG_ALBEDO", "0.0"))
albedo = mi.TensorXf(np.array([[ALBEDO, ALBEDO, ALBEDO]], dtype=np.float32).ravel(),
                     shape=(N, 3))

# Camera at (0, 0, -5) looking along +Z toward origin, ortho viewport 6x6.
# Mitsuba's orthographic camera with no scale on look_at sees [-1,1] in
# local X/Y; to get a 6×6 world viewport (= [-3,3]) we apply scale(3) to
# the camera's to_world.
cam_to_world = T().look_at(
    origin=[0, 0, -CAMERA_DISTANCE],
    target=[0, 0, 0],
    up=[0, 1, 0],
) @ T().scale([ORTHO_HEIGHT / 2.0] * 3)

# ── WS0/WS1: environment + camera selection (mirror test/scenes/single_gaussian.cpp) ──
# SG_ENV=meadow swaps the constant emitter for the real 4k HDR via an `envmap`.
# SG_PERSP=1 swaps the ortho sensor for a perspective one (needed to *see* the env
# background for orientation calibration; ortho collapses the background to 1 dir).
SG_ENV = os.environ.get("SG_ENV", "white_constant")
SG_PERSP = os.environ.get("SG_PERSP") == "1"
SG_FOV = float(os.environ.get("SG_FOV", "90"))
# SG_VIEW selects the camera viewing axis for full-sphere env calibration (mirror
# of test/scenes/single_gaussian.cpp view_config()).
_D = CAMERA_DISTANCE
_VIEWS = {
    "posz": ([0, 0, -_D], [0, 0, 0], [0, 1, 0]),
    "negz": ([0, 0, _D], [0, 0, 0], [0, 1, 0]),
    "posx": ([-_D, 0, 0], [0, 0, 0], [0, 1, 0]),
    "negx": ([_D, 0, 0], [0, 0, 0], [0, 1, 0]),
    "posy": ([0, -_D, 0], [0, 0, 0], [0, 0, 1]),
    "negy": ([0, _D, 0], [0, 0, 0], [0, 0, 1]),
}
SG_VIEW = os.environ.get("SG_VIEW", "posz")
_vo, _vt, _vu = _VIEWS.get(SG_VIEW, _VIEWS["posz"])
# Calibration DOF: rotation (deg) about +Y applied to the envmap to_world, plus an
# optional X flip, to match CUDA's u=atan2(z,x)/2π+0.5, v=acos(y)/π convention.
# Analytic prediction: ROTY=90 aligns azimuth; sweep to confirm empirically.
SG_ENV_ROTY = float(os.environ.get("SG_ENV_ROTY", "90"))
SG_ENV_FLIPX = os.environ.get("SG_ENV_FLIPX") == "1"

if SG_ENV == "meadow":
    env_to_world = T().rotate(axis=[0, 1, 0], angle=SG_ENV_ROTY)
    if SG_ENV_FLIPX:
        env_to_world = env_to_world @ T().scale([-1, 1, 1])
    environment_block = {
        "type": "envmap",
        "filename": os.path.join(THESIS_ROOT, "assets/meadow_2_4k.hdr"),
        "to_world": env_to_world,
    }
else:
    environment_block = {
        "type": "constant",
        "radiance": {"type": "uniform", "value": 1.0},
    }

if SG_PERSP:
    sensor_block = {
        "type": "perspective",
        "fov": SG_FOV,
        "fov_axis": "y",
        "near_clip": 0.01,
        "far_clip": 1000.0,
        "to_world": T().look_at(origin=_vo, target=_vt, up=_vu),
        "film": {
            "type": "hdrfilm",
            "width": WIDTH,
            "height": HEIGHT,
            "pixel_format": "rgb",
            "component_format": "float32",
            "filter": {"type": os.environ.get("SG_RFILTER", "gaussian")},
        },
        "sampler": {"type": "independent"},
    }
else:
    sensor_block = {
        "type": "orthographic",
        "near_clip": 0.01,
        "far_clip": 100.0,
        "film": {
            "type": "hdrfilm",
            "width": WIDTH,
            "height": HEIGHT,
            "pixel_format": "rgb",
            "component_format": "float32",
            "filter": {"type": os.environ.get("SG_RFILTER", "gaussian")},
        },
        "sampler": {"type": "independent"},
        "to_world": cam_to_world,
    }

scene_dict = {
    "type": "scene",
    "integrator": {
        "type": "volprim_prb",
        "max_depth": int(os.environ.get("SG_MAX_DEPTH", "32")),
        "kernel_type": "gaussian",
        "solver_type": "bisection",
        # NEE on by default for the scattering reference (faster convergence;
        # still unbiased). SG_NEE=0 forces it off.
        "use_nee": os.environ.get("SG_NEE", "1") == "1",
        # WS2: SG_HG_G≠0 switches the isotropic default to a Henyey-Greenstein
        # phase function (mirrors CUDA HG_G). volprim_prb reads 'phasefunction'.
        **({"phasefunction": {"type": "hg", "g": float(os.environ["SG_HG_G"])}}
           if os.environ.get("SG_HG_G") and float(os.environ.get("SG_HG_G", "0")) != 0.0
           else {}),
    },
    "primitive": {
        # SG_SHAPE=ellipsoids -> exact analytic ray-ellipsoid intersection (like CUDA);
        # default ellipsoidsmesh -> tessellated shell (uv_sphere, 72 tris).
        **({"type": "ellipsoids"}
           if os.environ.get("SG_SHAPE") == "ellipsoids"
           else {"type": "ellipsoidsmesh", "shell": os.environ.get("SG_SHELL", "uv_sphere")}),
        "centers": centers,
        "scales": scales,
        "quaternions": quaternions,
        "sigma_t": sigma_t,
        "albedo": albedo,
        "extent": 3.0,
    },
    "environment": environment_block,
    "sensor": sensor_block,
}

scene = mi.load_dict(scene_dict)
print(f"Rendering single Gaussian via volprim_tomography  "
      f"sigma_t_mitsuba={SIGMA_T_MITSUBA:.6f}  (equivalent CUDA sigma_multiplier={SIGMA_MULTIPLIER_CUDA})  spp={SPP}")
SEED = int(os.environ.get("SG_SEED", "0"))
img = mi.render(scene, sensor=scene.sensors()[0], spp=SPP, seed=SEED)
dr.sync_thread()

_tag = "_transformed" if os.environ.get("SG_TRANSFORMED") == "1" else ""
_albtag = f"_alb{ALBEDO:.2f}" if ALBEDO != 0.0 else ""
_seedtag = f"_seed{SEED}" if SEED != 0 else ""
_envtag = "_meadow" if SG_ENV == "meadow" else ""
_perstag = "_persp" if SG_PERSP else ""
_viewtag = f"_{SG_VIEW}" if SG_VIEW != "posz" else ""
_rottag = f"_roty{SG_ENV_ROTY:.0f}" if (SG_ENV == "meadow" and SG_ENV_ROTY != 0.0) else ""
_fliptag = "_flipx" if (SG_ENV == "meadow" and SG_ENV_FLIPX) else ""
_hg_env = os.environ.get("SG_HG_G")
_hgtag = f"_hg{float(_hg_env):.2f}" if (_hg_env and float(_hg_env) != 0.0) else ""
out = os.path.join(OUT_DIR, f"mitsuba_volprim_prb{_tag}{_envtag}{_perstag}{_viewtag}{_rottag}{_fliptag}{_hgtag}{_albtag}_M={SIGMA_T_MITSUBA:.3f}_spp{SPP}{_seedtag}.exr")
mi.Bitmap(img).write(out)
arr = np.array(img).astype(np.float32)
print(f"wrote {out}  mean={arr.mean():.4f}  max={arr.max():.4f}")
