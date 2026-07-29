#!/usr/bin/env python3
"""Capability smoke-test: render a synthetic dense density grid through Mitsuba's INDEPENDENT
heterogeneous/gridvolume/prbvolpath path tracer (the recipe in Jorge's render_voxel_grid.py).
This is the *render* half of a voxel-grid ground-truth pipeline; it shares no code with our
analytic-erf renderer or with volprim's analytic-Gaussian renderer.

Run:  experiments/mitsuba-reference/with_jorge_mitsuba.sh experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/voxel_smoke.py
"""
import numpy as np
import mitsuba as mi
mi.set_variant("cuda_ad_rgb")
T = mi.ScalarTransform4f

# --- synthetic density grid: a single 3D Gaussian blob in [0,1]^3 ---
R = 64
ax = (np.arange(R) + 0.5) / R
X, Y, Z = np.meshgrid(ax, ax, ax, indexing="ij")
c, s = 0.5, 0.16
blob = np.exp(-0.5 * (((X - c) ** 2 + (Y - c) ** 2 + (Z - c) ** 2) / s**2))
density = (25.0 * blob).astype(np.float32)  # peak sigma_t ~ 25
print(f"grid {R}^3  density: min {density.min():.3f} max {density.max():.3f} mean {density.mean():.3f}")

# --- scene: absorption only (albedo 0), constant white env, grid fills a cube ---
scene = mi.load_dict({
    "type": "scene",
    "integrator": {"type": "prbvolpath"},
    "object": {
        "type": "cube",
        "bsdf": {"type": "null"},
        "interior": {
            "type": "heterogeneous",
            "sigma_t": {"type": "gridvolume",
                        "data": mi.TensorXf(density),
                        "to_world": T().scale([2, 2, 2]).translate([-0.5, -0.5, -0.5])},
            "albedo": 0.0,
            "scale": 1.0,
        },
        "to_world": T().scale([1.0, 1.0, 1.0]),
    },
    "environment": {"type": "constant", "radiance": 1.0},
    "sensor": {
        "type": "perspective", "fov": 40,
        "to_world": T().look_at(origin=[0, 0, 4], target=[0, 0, 0], up=[0, 1, 0]),
        "film": {"type": "hdrfilm", "width": 256, "height": 256,
                 "pixel_format": "rgb", "rfilter": {"type": "box"}},
    },
})

img = mi.render(scene, spp=64)
arr = np.array(img)
print(f"render OK: shape {arr.shape}  mean {arr.mean():.4f}  "
      f"center {arr[128,128].mean():.4f}  corner {arr[10,10].mean():.4f}")
mi.util.write_bitmap("/home/kacper/thesis/results/campaign/voxel_smoke.exr", img)
mi.util.write_bitmap("/home/kacper/thesis/results/campaign/voxel_smoke.png", img)
print("wrote results/campaign/voxel_smoke.{exr,png}")
# sanity: absorption darkens the center (high density) vs the bright env corners
assert arr[128, 128].mean() < arr[10, 10].mean(), "center should be darker than corner (absorption)"
print("SANITY PASS: center darker than env corner (absorption working)")
