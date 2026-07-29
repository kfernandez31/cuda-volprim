#!/usr/bin/env python3
"""Tier A of the mini-core MRE: pure-absorber three-way comparison.
pred = exp(-tau_exact) per pixel (85-prim mini scene, scipy erf, support-clipped corrected formula)
vs ours (sandbox render) vs gabor (fork render). All through cam_0000.
Outputs: full-image + disputed-block stats, tau-space ratios, per-tau-bucket table.
Run under with_pip_gabor.sh with CLOUD_DIR = the MINI pkg.
"""
import os, sys, json
from os.path import join
import numpy as np
import mitsuba as mi
mi.set_variant("cuda_ad_rgb")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gabor_bootstrap  # noqa: F401
import volprim.integrators.volprim_prb  # noqa: F401
from mitsuba import ScalarTransform4f as T
from scipy.special import erf as serf

def _install_alias(cls):
    _has, _e1 = cls.has_attribute, cls.eval_attribute_1
    def has_attribute(self, name, active=True):
        return _has(self, "sigma_t" if name == "opacities" else name, active)
    def eval_attribute_1(self, name, si, active=True):
        return _e1(self, "sigma_t" if name == "opacities" else name, si, active)
    cls.has_attribute = has_attribute; cls.eval_attribute_1 = eval_attribute_1
for _c in (mi.Shape, mi.ShapePtr):
    _install_alias(_c)

PKG = os.environ.get("CLOUD_DIR", "results/campaign/nee_fair/minicore/pkg")
sys.path.insert(0, PKG)
import __init__ as cs
d = {"type": "scene"}
d.update(cs.OBJECTS); d.update(cs.EMITTERS); d.pop("resources", None)
d["primitives_pyr0"].pop("extent_adaptive_clamping", None)
d["primitives_pyr0"]["filename"] = join(PKG, "data/root.primitives_pyr0.ply")
d["environment"] = {"type": "constant", "radiance": {"type": "uniform", "value": 1.0}}
d["integrator"] = {"type": "volprim_prb", "max_depth": 128, "kernel_type": "gaussian",
                   "solver_type": "bisection", "use_nee": False}
cam = cs.SENSORS["cam_0000"].copy(); cam.pop("resources", None)
cam = {**cam, "film": {**cam["film"], "rfilter": {"type": "box"}}}
d["cam_0000"] = cam
sc = mi.load_dict(d)
params = mi.traverse(sc)
params["primitives_pyr0.sigma_t"] = params["primitives_pyr0.sigma_t"] * 7.5
params.update()
data = np.array(params["primitives_pyr0.data"]).reshape(-1, 10)
CTR, SCL, QUAT = data[:, 0:3], data[:, 3:6], data[:, 6:10]
SIG = np.array(params["primitives_pyr0.sigma_t"]).reshape(-1)
EXT = float(np.array(params["primitives_pyr0.extent"]))
N = len(CTR)
man = json.load(open(join(PKG, "manifest.json")))
LAYOUT = man["layout"]
print(f"[cmp] {N} prims, layout {LAYOUT}")

def quat_to_R(q, layout):
    x, y, z, w = (q[:, 0], q[:, 1], q[:, 2], q[:, 3]) if layout == "xyzw" else (q[:, 1], q[:, 2], q[:, 3], q[:, 0])
    n = np.sqrt(x*x + y*y + z*z + w*w); x, y, z, w = x/n, y/n, z/n, w/n
    R = np.empty((len(q), 3, 3))
    R[:, 0, 0] = 1 - 2*(y*y + z*z); R[:, 0, 1] = 2*(x*y - z*w);     R[:, 0, 2] = 2*(x*z + y*w)
    R[:, 1, 0] = 2*(x*y + z*w);     R[:, 1, 1] = 1 - 2*(x*x + z*z); R[:, 1, 2] = 2*(y*z - x*w)
    R[:, 2, 0] = 2*(x*z - y*w);     R[:, 2, 1] = 2*(y*z + x*w);     R[:, 2, 2] = 1 - 2*(x*x + y*y)
    return R
R = quat_to_R(QUAT, LAYOUT)

# camera rays for the FULL 600x900 grid (vectorized through the sensor)
H, W = 600, 900
sensor = sc.sensors()[0]
jj, ii = np.meshgrid(np.arange(W), np.arange(H))
pos = mi.Point2f(mi.Float((jj.ravel() + 0.5) / W), mi.Float((ii.ravel() + 0.5) / H))
rays, _ = sensor.sample_ray(mi.Float(0.0), mi.Float(0.5), pos, mi.Point2f(0.5, 0.5), mi.Bool(True))
O = np.stack([np.array(rays.o.x), np.array(rays.o.y), np.array(rays.o.z)], 1).astype(np.float64)
Dv = np.stack([np.array(rays.d.x), np.array(rays.d.y), np.array(rays.d.z)], 1).astype(np.float64)
Dv /= np.linalg.norm(Dv, axis=1, keepdims=True)

# exact tau per pixel: loop prims (85), vectorized over 540k rays
tau = np.zeros(len(O))
s2 = 1.0 / np.sqrt(2.0)
for i in range(N):
    Rt = R[i].T
    p = (O - CTR[i]) @ Rt.T / SCL[i]
    v = (Dv @ Rt.T) / SCL[i]
    vn = np.linalg.norm(v, axis=1); vhat = v / vn[:, None]
    B_ = (vhat * p).sum(1); C_ = (p * p).sum(1)
    disc = B_ * B_ - (C_ - EXT * EXT)
    ok = disc > 0
    sq = np.sqrt(np.maximum(disc, 0))
    t0 = np.maximum((-B_ - sq) / vn, 0.0); t1 = (-B_ + sq) / vn
    ok &= t1 > t0
    ps = p + (t0 * vn)[:, None] * vhat
    L_ = (t1 - t0) * vn
    B = (vhat * ps).sum(1); C = (ps * ps).sum(1)
    erf_term = 0.5 * (serf(s2 * (L_ + B)) - serf(s2 * B))
    D = (1.0 / vn) * np.exp(-0.5 * (C - B * B)) * erf_term / (2.0 * np.pi * SCL[i].prod())
    tau += np.where(ok, SIG[i] * np.maximum(D, 0.0), 0.0)
pred = np.exp(-tau).reshape(H, W)
tau = tau.reshape(H, W)

def load_exr(p):
    bmp = mi.Bitmap(p)
    a = np.array(bmp, dtype=np.float64)
    return a[..., :3].mean(-1)

ours = load_exr(os.environ.get("OURS_EXR", "results/campaign/nee_fair/minicore/sandbox/test_results/cloud_asset_validation/0000.exr"))
gab = load_exr(os.environ.get("GABOR_EXR", "results/campaign/nee_fair/minicore/gabor_analog_white_constant_spp2048_seed0.exr"))
assert ours.shape == pred.shape == gab.shape, (ours.shape, pred.shape, gab.shape)

r0, r1, c0, c1 = man["block"]
def stats(name, img):
    fm = img.mean(); bm = img[r0:r1, c0:c1].mean()
    print(f"{name:8s} full-mean {fm:.5f}   block-mean {bm:.5f}")
    return bm
print(f"\n=== Tier A absorber: exp(-tau)*bg, {N}-prim mini scene, cam_0000 ===")
pb = stats("exact", pred); ob = stats("ours", ours); gb = stats("gabor", gab)
print(f"\nblock ratios: ours/exact {ob/pb:.4f}   gabor/exact {gb/pb:.4f}   ours/gabor {ob/gb:.4f}")

# tau-space per-bucket comparison (pixels grouped by exact tau)
to = -np.log(np.clip(ours, 1e-12, 1)); tg = -np.log(np.clip(gab, 1e-12, 1))
print(f"\n{'tau bucket':>12s} {'npix':>7s} {'tau_exact':>10s} {'tau_ours':>9s} {'tau_gabor':>10s} {'ours/ex':>8s} {'gab/ex':>7s}")
for lo, hi in [(0.02, 0.1), (0.1, 0.3), (0.3, 1.0), (1.0, 2.0), (2.0, 4.0), (4.0, 8.0)]:
    m = (tau > lo) & (tau <= hi)
    if m.sum() < 20: continue
    print(f"{lo:5.2f}-{hi:5.2f} {m.sum():7d} {tau[m].mean():10.4f} {to[m].mean():9.4f} {tg[m].mean():10.4f} "
          f"{to[m].mean()/tau[m].mean():8.4f} {tg[m].mean()/tau[m].mean():7.4f}")

np.save("results/campaign/nee_fair/minicore/tau_exact.npy", tau)
print("\n[cmp] saved tau_exact.npy")
