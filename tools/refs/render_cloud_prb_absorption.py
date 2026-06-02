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

SIGMAT_SCALE = 7.5            # matches CUDA --sigma-multiplier 7.5
SPP = int(os.environ.get('SG_SPP', '64'))
SEED = int(os.environ.get('SG_SEED', '0'))
ONE_CAM = os.environ.get('SG_CAM')  # e.g. "0" to render only cam_0000

scene_dict = {'type': 'scene'}
scene_dict.update(cloud_scene.OBJECTS)
scene_dict.update(cloud_scene.EMITTERS)
if 'resources' in scene_dict:
    del scene_dict['resources']
prim = scene_dict.get('primitives_pyr0', {})
if 'extent_adaptive_clamping' in prim:
    del prim['extent_adaptive_clamping']
scene_dict['primitives_pyr0']['filename'] = join(CLOUD_DIR, 'data/root.primitives_pyr0.ply')
# Force absorption path-tracer behavior (no NEE; albedo stays at PLY≈0).
scene_dict['integrator'] = {'type': 'volprim_prb', 'max_depth': 32,
                            'kernel_type': 'gaussian', 'solver_type': 'bisection',
                            'use_nee': False}

out_dir = join(CLOUD_DIR, 'refs_prb_absorption')
os.makedirs(out_dir, exist_ok=True)

cams = sorted(k for k in cloud_scene.SENSORS if k.startswith('cam_'))
if ONE_CAM is not None:
    cams = [f'cam_{int(ONE_CAM):04d}']
print(f"prb absorption ref: sigmat_scale={SIGMAT_SCALE} albedo=PLY(≈0) spp={SPP} shape="
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
    # NO albedo override — keep PLY albedo (≈0) for pure absorption.
    params.update()
    cam_idx = int(cam_name.split('_')[1])
    img = mi.render(scene, sensor=scene.sensors()[0], spp=SPP, seed=SEED)
    out = join(out_dir, f'{cam_idx:04d}.exr')
    mi.util.write_bitmap(out, img)
    arr = np.array(img).astype(np.float32)
    print(f"[{i+1}/{len(cams)}] {cam_name} -> {out}  mean={arr.mean():.4f} min={arr.min():.4f}")
print("DONE")
