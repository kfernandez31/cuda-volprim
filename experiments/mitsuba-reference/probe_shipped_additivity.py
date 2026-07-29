#!/usr/bin/env python3
"""Segment-additivity probe on the SHIPPED volprim release (622fabd) GaussianKernel.
Question: did the released kernel's finite-bounds segment integral satisfy
tau(a,b) + tau(b,c) == tau(a,c)?  (The GaborVolumes rewrite violated it 5.85x
via the mirrored-B sign convention; this decides whether that class of issue
was present in the release.)
Run: tools/refs/with_jorge_mitsuba.sh tools/refs/.venv/bin/python tools/refs/probe_shipped_additivity.py
"""
import numpy as np
import mitsuba as mi
mi.set_variant("cuda_ad_rgb")
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


# asymmetric segments off-centre (the configuration that exposed the mirror in the rewrite)
o, d = [0.3, 0.0, -2.0], [0.0, 0.0, 1.0]
a, b, c = 0.5, 1.7, 3.0
s_ab, s_bc, s_ac = seg(o, d, a, b), seg(o, d, b, c), seg(o, d, a, c)
print(f"ADDITIVITY [{a},{b}]+[{b},{c}] = {s_ab + s_bc:.6f}   [{a},{c}] = {s_ac:.6f}   "
      f"factor {(s_ab + s_bc) / s_ac:.4f}  (1.0000 = pass)")

# a second, harsher split (segment centred vs started on the closest approach)
a2, b2, c2 = 1.2, 2.0, 2.8
s2 = seg(o, d, a2, b2), seg(o, d, b2, c2), seg(o, d, a2, c2)
print(f"ADDITIVITY [{a2},{b2}]+[{b2},{c2}] = {s2[0] + s2[1]:.6f}   [{a2},{c2}] = {s2[2]:.6f}   "
      f"factor {(s2[0] + s2[1]) / s2[2]:.4f}  (1.0000 = pass)")
