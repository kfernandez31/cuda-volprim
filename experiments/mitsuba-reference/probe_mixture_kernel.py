#!/usr/bin/env python3
"""Internal-consistency probe on volprim's GaussianGaborMixtureKernel (the production kernel).
Answers Jorge's question: does the segment-sum issue also affect the Gabor Gaussian mixture kernel?

CHECK 1: segment additivity [a,b]+[b,c] == [a,c] — pure function property, normalisation-free.
         Run for BOTH branches: is_gabor=False (pure Gaussian) and is_gabor=True (beta != 0).
CHECK 2: ratio flatness vs brute-force quadrature of the kernel's own eval(): for a correct
         integral, density_integral([t0, t0+dt]) / quadrature_of_eval([t0, t0+dt]) is the SAME
         constant for every t0 (whatever the normalisation). A mirrored integral makes the
         ratio swing with t0.

Run: VOLPRIM_DIR=/path/to/tree tools/refs/with_pip_gabor.sh tools/refs/.venv/bin/python \
     tools/refs/probe_mixture_kernel.py
"""
import os, sys
import numpy as np
import mitsuba as mi
mi.set_variant("cuda_ad_rgb")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    import gabor_bootstrap  # noqa: F401  (shim for running under pip mitsuba; not needed in Jorge's env)
except ImportError:
    pass
from volprim.integrators.common import Ellipsoid, GaussianGaborMixtureKernel

sc = mi.load_dict({"type": "scene",
    "p": {"type": "ellipsoids", "centers": mi.TensorXf([0, 0, 0], shape=(1, 3)),
          "scales": mi.TensorXf([1, 1, 1], shape=(1, 3)),
          "quaternions": mi.TensorXf([0, 0, 0, 1], shape=(1, 4)),
          "opacities": mi.TensorXf([6.0], shape=(1, 1)),
          "albedo": mi.TensorXf([1, 1, 1], shape=(1, 3)), "extent": 3.0},
    "e": {"type": "constant", "radiance": {"type": "uniform", "value": 1.0}}})
ell = Ellipsoid.gather(sc.shapes_dr(), mi.UInt32(0), mi.Bool(True))
kern = GaussianGaborMixtureKernel(mi.Properties())
BETA = 2.0

def seg(o, d, t0, t1, is_gabor):
    ray = mi.Ray3f(mi.Point3f(*o), mi.Vector3f(*d)); ray.maxt = mi.Float(1e30)
    return float(np.array(kern.density_integral(ray, ell, mi.Bool(is_gabor),
                                                tmin=mi.Float(t0), tmax=mi.Float(t1),
                                                active=mi.Bool(True)))[0])

def quad(o, d, t0, t1, is_gabor):
    """Brute-force quadrature of the kernel's own eval() formula (unit isotropic ellipsoid at
    origin: whitened point == world point)."""
    ts = np.linspace(t0, t1, 200000)
    x = np.array(o)[None] + ts[:, None] * np.array(d)[None]
    dens = np.exp(-0.5 * (x * x).sum(1))
    if is_gabor:
        dens *= np.cos(BETA * x.sum(1))
    return np.trapezoid(dens, ts)

o, d = [0.3, 0.0, -2.0], [0.0, 0.0, 1.0]
a, b, c = 0.5, 1.7, 3.0
for is_gabor, tag in ((False, "Gaussian branch (is_gabor=False)"),
                      (True,  f"Gabor branch (is_gabor=True, beta={BETA})")):
    if is_gabor:
        ell.beta = mi.Float(BETA)
    s_ab, s_bc, s_ac = (seg(o, d, a, b, is_gabor), seg(o, d, b, c, is_gabor),
                        seg(o, d, a, c, is_gabor))
    print(f"[{tag}]")
    print(f"  CHECK 1 additivity: [{a},{b}]+[{b},{c}] = {s_ab+s_bc:.6f}   [{a},{c}] = {s_ac:.6f}"
          f"   factor {(s_ab+s_bc)/s_ac:.3f}  (must be 1.000)")
    ratios = []
    for t0 in (0.5, 1.5, 2.5):
        si = seg(o, d, t0, t0 + 0.5, is_gabor)
        qi = quad(o, d, t0, t0 + 0.5, is_gabor)
        ratios.append(si / qi)
        print(f"  CHECK 2 [{t0},{t0+0.5}]: density_integral={si:.6f}  quad(own eval)={qi:.6f}"
              f"  ratio={si/qi:+.4f}")
    r = np.array(ratios)
    print(f"  CHECK 2 verdict: ratio spread {r.min():+.4f} .. {r.max():+.4f} "
          f"(correct integral = constant ratio)")
