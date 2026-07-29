#!/usr/bin/env python3
"""Internal-consistency probes on volprim's GaussianKernel — no cross-renderer comparison involved.
CHECK 1: segment additivity  [a,b]+[b,c] == [a,c]   (violated ~5.9x in the shipped code)
CHECK 2: inv_cdf vs density_integral round trip     (13-60x apart in the shipped code)
Run: VOLPRIM_DIR=/path/to/volprim-tree ./with_pip_gabor.sh python probe_internal_consistency.py
"""
import os, sys
import numpy as np
import mitsuba as mi
mi.set_variant("cuda_ad_rgb")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gabor_bootstrap  # noqa: F401
from volprim.integrators.common import Ellipsoid, GaussianKernel

sc = mi.load_dict({"type": "scene",
    "p": {"type": "ellipsoids", "centers": mi.TensorXf([0, 0, 0], shape=(1, 3)),
          "scales": mi.TensorXf([1, 1, 1], shape=(1, 3)),
          "quaternions": mi.TensorXf([0, 0, 0, 1], shape=(1, 4)),
          "opacities": mi.TensorXf([6.0], shape=(1, 1)),
          "albedo": mi.TensorXf([1, 1, 1], shape=(1, 3)), "extent": 3.0},
    "e": {"type": "constant", "radiance": {"type": "uniform", "value": 1.0}}})
ell = Ellipsoid.gather(sc.shapes_dr(), mi.UInt32(0), mi.Bool(True))
kern = GaussianKernel(mi.Properties())
def seg(o, d, t0, t1):
    ray = mi.Ray3f(mi.Point3f(*o), mi.Vector3f(*d)); ray.maxt = mi.Float(1e30)
    return float(np.array(kern.density_integral(ray, ell, tmin=mi.Float(t0), tmax=mi.Float(t1),
                                                active=mi.Bool(True)))[0])
o, d = [0.3, 0.0, -2.0], [0.0, 0.0, 1.0]
a, b, c = 0.5, 1.7, 3.0
s_ab, s_bc, s_ac = seg(o, d, a, b), seg(o, d, b, c), seg(o, d, a, c)
print(f"CHECK 1 additivity: [{a},{b}]+[{b},{c}] = {s_ab+s_bc:.6f}   [{a},{c}] = {s_ac:.6f}   "
      f"factor {(s_ab+s_bc)/s_ac:.3f} (must be 1.000)")
ray = mi.Ray3f(mi.Point3f(*o), mi.Vector3f(*d)); ray.maxt = mi.Float(1e30)
for chi in (0.1, 0.3, 0.5):
    t_s = float(np.array(kern.inv_cdf(ray, ell, mi.Float(6.0), mi.Float(chi), mi.Bool(True)))[0])
    tau_seg = 6.0 * seg(o, d, 0.0, t_s)
    ts = np.linspace(0, t_s, 200000)
    x = np.array(o)[None] + ts[:, None] * np.array(d)[None]
    tau_quad = 6.0 * np.trapezoid(np.exp(-0.5 * (x * x).sum(1)), ts) / (2 * np.pi)
    print(f"CHECK 2 chi={chi}: t_s={t_s:.4f}  sigma*segment[0,t_s]={tau_seg:.5f}  "
          f"sigma*quadrature[0,t_s]={tau_quad:.5f}  (proportional to chi = inv_cdf correct; "
          f"flat = segment branch inconsistent)")
