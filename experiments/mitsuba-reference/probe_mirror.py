# Verify the B-sign (mirror) hypothesis on GaussianKernel.density_integral's segment branch.
# Normalization-free: compare code_seg/code_full vs quad_seg/quad_full vs mirror-formula prediction.
import os, sys, math
import numpy as np
import mitsuba as mi
mi.set_variant("cuda_ad_rgb")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gabor_bootstrap  # noqa
import drjit as dr
from volprim.integrators.common import Ellipsoid, GaussianKernel

sc = mi.load_dict({"type": "scene",
    "p": {"type": "ellipsoids",
          "centers": mi.TensorXf([0, 0, 0], shape=(1, 3)),
          "scales": mi.TensorXf([1, 1, 1], shape=(1, 3)),
          "quaternions": mi.TensorXf([0, 0, 0, 1], shape=(1, 4)),
          "opacities": mi.TensorXf([6.0], shape=(1, 1)),
          "albedo": mi.TensorXf([1, 1, 1], shape=(1, 3)), "extent": 3.0},
    "e": {"type": "constant", "radiance": {"type": "uniform", "value": 1.0}}})
shape = sc.shapes_dr()
ell = Ellipsoid.gather(shape, mi.UInt32(0), mi.Bool(True))
kern = GaussianKernel({"kernel_full_range": False} if False else mi.Properties())

def code_seg(o, d, t0, t1):
    ray = mi.Ray3f(mi.Point3f(*o), mi.Vector3f(*d))
    ray.maxt = mi.Float(1e30)
    v = kern.density_integral(ray, ell, tmin=mi.Float(t0), tmax=mi.Float(t1), active=mi.Bool(True))
    return float(np.array(v)[0])

def code_full(o, d):
    ray = mi.Ray3f(mi.Point3f(*o), mi.Vector3f(*d))
    v = kern.density_integral(ray, ell, tmin=None, tmax=None, active=mi.Bool(True))
    return float(np.array(v)[0])

def quad(o, d, t0, t1, n=400000):
    o, d = np.array(o, float), np.array(d, float)
    t = np.linspace(t0, t1, n); x = o[None] + t[:, None] * d[None]
    g = np.exp(-0.5 * (x * x).sum(1))
    return np.trapezoid(g, t)

ERF = math.erf
def mirror_pred(o, d, t0, t1):
    """the claimed code behavior: mirror B about segment start, /sqrt(2pi) vs full=1"""
    o, d = np.array(o, float), np.array(d, float)
    dn = d / np.linalg.norm(d)
    p = o + t0 * dn
    B = float(dn @ p); C = float(p @ p); L = min(t1 - t0, 6.0)
    s = 1 / math.sqrt(2)
    num = 0.5 * (ERF(s * B) + ERF(s * (L - B)))       # mirrored combo
    den = 1.0                                          # full-range: exp(-(C-B2)/2), same factor cancels
    return num / den

def true_pred(o, d, t0, t1):
    o, d = np.array(o, float), np.array(d, float)
    dn = d / np.linalg.norm(d)
    p = o + t0 * dn
    B = float(dn @ p); L = min(t1 - t0, 6.0)
    s = 1 / math.sqrt(2)
    return 0.5 * (ERF(s * (L + B)) - ERF(s * B))

cases = [
    ("B<0 centre ahead", [0.3, 0, -2.0], [0, 0, 1], 0.5, 3.0),
    ("B=0 at foot",      [0.3, 0,  0.0], [0, 0, 1], 0.0, 2.5),
    ("B>0 centre behind",[0.3, 0,  1.0], [0, 0, 1], 0.2, 2.2),
    ("oblique",          [0.5, -0.4, -1.5], [0.2, 0.1, 1.0], 0.3, 2.8),
]
print(f"{'case':22s} {'code_ratio':>11s} {'quad_ratio':>11s} {'mirror_pred':>12s} {'true_pred':>10s}")
for name, o, d, t0, t1 in cases:
    cf = code_full(o, d)
    cr = code_seg(o, d, t0, t1) / cf
    qr = quad(o, d, t0, t1) / quad(o, d, -60, 60)
    mp = mirror_pred(o, d, t0, t1)
    tp = true_pred(o, d, t0, t1)
    print(f"{name:22s} {cr:11.6f} {qr:11.6f} {mp:12.6f} {tp:10.6f}")
