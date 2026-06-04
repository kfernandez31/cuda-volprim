#!/usr/bin/env python3
"""
Matched Mitsuba volprim_prb reference for cloud_asset_validation (CUDA side):
ABSORPTION config — albedo=0 (PLY values, ≈0), sigmat_scale=7.5 — using the
cloud scene's native analytic 'ellipsoids' shape and the same 24 cameras.

This is the apples-to-apples reference the old refs_prb_pyr0/ was NOT (those
used albedo=0.9 scattering + sigmat 60). Renders to assets/cloud/refs_prb_absorption/.

Run:
  source tools/refs/.venv/bin/activate
  tools/refs/with_jorge_mitsuba.sh tools/refs/.venv/bin/python tools/refs/render_cloud_prb_absorption.py
Env: SG_SPP (default 64), SG_CAM (optional single cam index for a quick check).
"""
import os
import sys
from os.path import join, dirname, abspath

sys.path.insert(0, '/home/kacper/volumetric_primitives')
import mitsuba as mi
mi.set_variant('cuda_ad_rgb')
import volprim.integrators.volprim_prb  # noqa: F401
import numpy as np

CLOUD_DIR = '/home/kacper/thesis/assets/cloud'
sys.path.insert(0, CLOUD_DIR)
import __init__ as cloud_scene

SIGMAT_SCALE = float(os.environ.get('SG_SIGMA', '7.5'))   # matches CUDA --sigma-multiplier
SPP = int(os.environ.get('SG_SPP', '64'))
SEED = int(os.environ.get('SG_SEED', '0'))
ONE_CAM = os.environ.get('SG_CAM')  # e.g. "0" to render only cam_0000
# SG_ALBEDO>0 turns this into the SCATTERING reference (overrides PLY albedo≈0).
# use_nee stays False (analog = trustworthy; prb's NEE fails the furnace test +6.5%).
# SG_MAX_DEPTH default 128 to match CUDA MAX_BOUNCES (depth matters in deep media —
# at depth 32 the dense cloud loses multiply-scattered energy; see FINDINGS §8.3).
ALBEDO = float(os.environ.get('SG_ALBEDO', '0.0'))
MAX_DEPTH = int(os.environ.get('SG_MAX_DEPTH', '128'))

scene_dict = {'type': 'scene'}
scene_dict.update(cloud_scene.OBJECTS)
scene_dict.update(cloud_scene.EMITTERS)
if 'resources' in scene_dict:
    del scene_dict['resources']

# WS1/WS4: SG_ENV=meadow swaps the constant emitter for the real 4k HDR. The env
# is in WORLD space and both renderers apply it in world space, so the roty90
# orientation calibration (FINDINGS §8.6) carries over unchanged to the cloud's
# 24 world-frame cameras.
SG_ENV = os.environ.get('SG_ENV', 'white_constant')
SG_ENV_ROTY = float(os.environ.get('SG_ENV_ROTY', '90'))
if SG_ENV == 'meadow':
    from mitsuba import ScalarTransform4f as T
    scene_dict['environment'] = {
        'type': 'envmap',
        'filename': '/home/kacper/thesis/assets/meadow_2_4k.hdr',
        'to_world': T().rotate(axis=[0, 1, 0], angle=SG_ENV_ROTY),
    }
prim = scene_dict.get('primitives_pyr0', {})
if 'extent_adaptive_clamping' in prim:
    del prim['extent_adaptive_clamping']
scene_dict['primitives_pyr0']['filename'] = join(CLOUD_DIR, 'data/root.primitives_pyr0.ply')
# SG_NEE=1 turns ON Mitsuba's own NEE/MIS (its importance-sampled path — fast but energy-biased,
# fails the furnace by +6.5%, FINDINGS §8.1). Default 0 = analog (the trustworthy reference).
# Used to benchmark CUDA-MIS against Mitsuba's *own* MIS apples-to-apples.
USE_NEE = os.environ.get('SG_NEE', '0') == '1'
scene_dict['integrator'] = {'type': 'volprim_prb', 'max_depth': MAX_DEPTH,
                            'kernel_type': 'gaussian', 'solver_type': 'bisection',
                            'use_nee': USE_NEE}
# WS2/WS4: SG_HG_G≠0 swaps the isotropic default for Henyey-Greenstein (mirrors CUDA HG_G).
_hg = os.environ.get('SG_HG_G')
if _hg and float(_hg) != 0.0:
    scene_dict['integrator']['phasefunction'] = {'type': 'hg', 'g': float(_hg)}

_envtag = '_meadow' if SG_ENV == 'meadow' else ''
_hgtag = f'_hg{float(_hg):.2f}' if (_hg and float(_hg) != 0.0) else ''
_neetag = '_nee' if USE_NEE else ''
out_dir = join(CLOUD_DIR, ('refs_prb_scattering' if ALBEDO > 0.0 else 'refs_prb_absorption') + _envtag + _hgtag + _neetag)
os.makedirs(out_dir, exist_ok=True)

cams = sorted(k for k in cloud_scene.SENSORS if k.startswith('cam_'))
if ONE_CAM is not None:
    cams = [f'cam_{int(ONE_CAM):04d}']
_mode = f"SCATTERING albedo={ALBEDO}" if ALBEDO > 0.0 else "absorption albedo=PLY(≈0)"
print(f"prb {_mode} ref: sigmat_scale={SIGMAT_SCALE} max_depth={MAX_DEPTH} spp={SPP} shape="
      f"{scene_dict['primitives_pyr0']['type']}  cams={len(cams)}")

for i, cam_name in enumerate(cams):
    d = scene_dict.copy()
    cc = cloud_scene.SENSORS[cam_name].copy()
    cc.pop('resources', None)
    # Optional pixel reconstruction filter override (CUDA uses a box filter; the
    # hdrfilm default is gaussian — set SG_RFILTER=box to match CUDA exactly).
    rfilt = os.environ.get('SG_RFILTER')
    if rfilt:
        cc = {**cc, 'film': {**cc['film'], 'rfilter': {'type': rfilt}}}
    d[cam_name] = cc
    scene = mi.load_dict(d)
    params = mi.traverse(scene)
    params['primitives_pyr0.sigma_t'] = params['primitives_pyr0.sigma_t'] * SIGMAT_SCALE
    if ALBEDO > 0.0:
        # Override per-prim albedo to a uniform constant (×0+c preserves dtype/shape).
        params['primitives_pyr0.albedo'] = params['primitives_pyr0.albedo'] * 0.0 + ALBEDO
    params.update()
    cam_idx = int(cam_name.split('_')[1])
    img = mi.render(scene, sensor=scene.sensors()[0], spp=SPP, seed=SEED)
    out = join(out_dir, f'{cam_idx:04d}.exr')
    mi.util.write_bitmap(out, img)
    arr = np.array(img).astype(np.float32)
    print(f"[{i+1}/{len(cams)}] {cam_name} -> {out}  mean={arr.mean():.4f} min={arr.min():.4f}")
print("DONE")
