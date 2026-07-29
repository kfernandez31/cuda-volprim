#!/usr/bin/env python3
"""Mini-core MRE extractor: select the prims that carry the optical depth of the disputed
core block (camera chords + vertex->sun chords) and write a standalone mini asset package
loadable by BOTH renderers' existing cloud harnesses:
  ours : sandbox CWD with assets/models/cloud -> <pkg>   (root.primitives_pyr0.ply at top level)
  gabor: CLOUD_DIR=<pkg>                                  (data/root.primitives_pyr0.ply)
Selection is done on mitsuba-DECODED params (no PLY-decode conventions in play); the PLY is
filtered by ROW INDEX with plyfile (bytes preserved verbatim for kept rows).

Env: SG_EPS (per-prim tau threshold, default 2e-3), SG_OUT (pkg dir,
     default results/campaign/nee_fair/minicore/pkg). Run under with_pip_gabor.sh from repo root.
"""
import os, sys, json, shutil
from os.path import join
import numpy as np
import mitsuba as mi
mi.set_variant("cuda_ad_rgb")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gabor_bootstrap  # noqa: F401
import drjit as dr
import volprim.integrators.volprim_prb  # noqa: F401  (registers the volprim_prb plugin)
from mitsuba import ScalarTransform4f as T
from math import erf as _erf, sqrt, pi

def _install_alias(cls):
    _has, _e1 = cls.has_attribute, cls.eval_attribute_1
    def has_attribute(self, name, active=True):
        return _has(self, "sigma_t" if name == "opacities" else name, active)
    def eval_attribute_1(self, name, si, active=True):
        return _e1(self, "sigma_t" if name == "opacities" else name, si, active)
    cls.has_attribute = has_attribute; cls.eval_attribute_1 = eval_attribute_1
for _c in (mi.Shape, mi.ShapePtr):
    _install_alias(_c)

CLOUD = os.environ.get("CLOUD_DIR", "assets/models/cloud")
sys.path.insert(0, CLOUD)
import __init__ as cs
d = {"type": "scene"}
d.update(cs.OBJECTS); d.update(cs.EMITTERS); d.pop("resources", None)
d["primitives_pyr0"].pop("extent_adaptive_clamping", None)
d["primitives_pyr0"]["filename"] = join(CLOUD, "data/root.primitives_pyr0.ply")
d["environment"] = {"type": "envmap",
                    "filename": os.environ.get("MEADOW_HDR", "assets/environment_maps/meadow_2_4k.hdr"),
                    "to_world": T().rotate(axis=[0, 1, 0], angle=90.0)}
d["integrator"] = {"type": "volprim_prb", "max_depth": 128, "kernel_type": "gaussian",
                   "solver_type": "bisection", "use_nee": True,
                   "phasefunction": {"type": "hg", "g": 0.85}}
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
print(f"[extract] {N} prims, extent {EXT}")

def quat_to_R(q, layout):
    x, y, z, w = (q[:, 0], q[:, 1], q[:, 2], q[:, 3]) if layout == "xyzw" else (q[:, 1], q[:, 2], q[:, 3], q[:, 0])
    n = np.sqrt(x*x + y*y + z*z + w*w); x, y, z, w = x/n, y/n, z/n, w/n
    R = np.empty((len(q), 3, 3))
    R[:, 0, 0] = 1 - 2*(y*y + z*z); R[:, 0, 1] = 2*(x*y - z*w);     R[:, 0, 2] = 2*(x*z + y*w)
    R[:, 1, 0] = 2*(x*y + z*w);     R[:, 1, 1] = 1 - 2*(x*x + z*z); R[:, 1, 2] = 2*(y*z - x*w)
    R[:, 2, 0] = 2*(x*z - y*w);     R[:, 2, 1] = 2*(y*z + x*w);     R[:, 2, 2] = 1 - 2*(x*x + y*y)
    return R

ERFV = np.vectorize(_erf)
from volprim.integrators.common import Ellipsoid

def tau_per_prim(o, w_dir, R, tmax=np.inf):
    """Per-prim exact optical depth along o + t*w_dir (unit), t in [0,tmax], support-clipped."""
    p = np.einsum("nij,nj->ni", np.transpose(R, (0, 2, 1)), (o[None] - CTR)) / SCL
    v = np.einsum("nij,j->ni", np.transpose(R, (0, 2, 1)), w_dir) / SCL
    vn = np.linalg.norm(v, axis=1); vhat = v / vn[:, None]
    B_ = (vhat * p).sum(1); C_ = (p * p).sum(1)
    disc = B_*B_ - (C_ - EXT*EXT)
    ok = disc > 0
    su0 = -B_ - np.sqrt(np.maximum(disc, 0)); su1 = -B_ + np.sqrt(np.maximum(disc, 0))
    t0 = np.maximum(su0 / vn, 0.0); t1 = np.minimum(su1 / vn, tmax)
    ok &= t1 > t0
    ps = p + (t0 * vn)[:, None] * vhat
    L_ = (t1 - t0) * vn
    B = (vhat * ps).sum(1); C = (ps * ps).sum(1)
    s2 = 1.0 / sqrt(2.0)
    erf_term = 0.5 * (ERFV(s2 * (L_ + B)) - ERFV(s2 * B))
    D = (1.0 / vn) * np.exp(-0.5 * (C - B * B)) * erf_term / (2.0 * pi * SCL.prod(1))
    return np.where(ok, SIG * np.maximum(D, 0.0), 0.0)

# quat layout self-test (vs kernel full-range, layout with lower err wins)
kern = sc.integrator().kernel
rng = np.random.default_rng(0)
best = None
for layout in ("xyzw", "wxyz"):
    R = quat_to_R(QUAT, layout); errs = []
    for _ in range(24):
        i = int(rng.integers(0, N))
        o = CTR[i] + rng.normal(0, 1, 3) * SCL[i] * 4.0
        w = rng.normal(0, 1, 3); w /= np.linalg.norm(w)
        ell = Ellipsoid.gather(sc.shapes_dr(), mi.UInt32(i), mi.Bool(True))
        ray = mi.Ray3f(mi.Point3f(*o.tolist()), mi.Vector3f(*w.tolist()))
        Dk = float(np.array(kern.density_integral(ray, ell, tmin=None, tmax=None, active=mi.Bool(True)))[0])
        p = (np.transpose(R[i]) @ (o - CTR[i])) / SCL[i]
        v = (np.transpose(R[i]) @ w) / SCL[i]
        vn = np.linalg.norm(v)
        B = float((v / vn) @ p); C = float(p @ p)
        Dn = (1.0 / vn) * np.exp(-0.5 * (C - B * B)) / (2.0 * pi * SCL[i].prod())
        if Dk > 1e-12:
            errs.append(abs(Dn - Dk) / Dk)
    m = float(np.median(errs)) if errs else 9e9
    print(f"[self-test] layout {layout}: median rel err = {m:.2e}")
    if best is None or m < best[1]:
        best = (layout, m)
LAYOUT, err = best
assert err < 1e-3, f"layout self-test failed ({err})"
R = quat_to_R(QUAT, LAYOUT)
print(f"[self-test] using layout {LAYOUT}")

# sun direction
em = sc.environment()
ref = dr.zeros(mi.Interaction3f); ref.p = mi.Point3f(0, 0, 0)
smp = mi.load_dict({"type": "independent"}); smp.seed(7, 8192)
ds, wgt = em.sample_direction(ref, smp.next_2d(), mi.Bool(True))
rad = np.array(wgt).mean(0) * np.array(ds.pdf)
k = int(np.argmax(rad))
SUN = np.array([np.array(ds.d.x)[k], np.array(ds.d.y)[k], np.array(ds.d.z)[k]])
print(f"[extract] sun {SUN.round(4)}")

# probe rays: camera chords through the disputed block + sun chords from tau_cam in {0.5,1,2} vertices
sensor = sc.sensors()[0]
r0, r1, c0, c1 = 480, 540, 480, 540
probes = []          # (origin, dir, tmax)
for r in range(r0 + 5, r1, 10):
    for c in range(c0 + 5, c1, 10):
        pos = mi.Point2f((c + 0.5) / 900.0, (r + 0.5) / 600.0)
        cray, _ = sensor.sample_ray(0.0, 0.5, pos, mi.Point2f(0.5, 0.5), mi.Bool(True))
        co = np.array([float(np.array(cray.o.x)[0]), float(np.array(cray.o.y)[0]), float(np.array(cray.o.z)[0])])
        cd = np.array([float(np.array(cray.d.x)[0]), float(np.array(cray.d.y)[0]), float(np.array(cray.d.z)[0])])
        cd /= np.linalg.norm(cd)
        probes.append((co, cd, np.inf))
        for TAU in (0.5, 1.0, 2.0):
            lo, hi = 0.0, 40.0
            if tau_per_prim(co, cd, R, tmax=hi).sum() < TAU:
                continue
            for _ in range(36):
                mid = 0.5 * (lo + hi)
                lo, hi = (mid, hi) if tau_per_prim(co, cd, R, tmax=mid).sum() < TAU else (lo, mid)
            probes.append((co + cd * hi, SUN, np.inf))
print(f"[extract] {len(probes)} probe rays (camera + sun chords)")

TP = np.stack([tau_per_prim(o, w, R, tmax=t) for (o, w, t) in probes])  # (P, N)
EPS = float(os.environ.get("SG_EPS", "2e-3"))
sel = np.where(TP.max(0) > EPS)[0]
cov_cam = np.array([TP[i, sel].sum() / max(TP[i].sum(), 1e-12) for i in range(len(probes))])
print(f"[extract] EPS={EPS}: {len(sel)} prims selected; per-probe tau coverage "
      f"min {cov_cam.min():.4f} / median {np.median(cov_cam):.4f}")
if cov_cam.min() < 0.98:
    print("[extract] WARNING coverage <0.98 — consider lowering SG_EPS")

# write the mini package
OUT = os.environ.get("SG_OUT", "results/campaign/nee_fair/minicore/pkg")
os.makedirs(join(OUT, "data"), exist_ok=True)
for f in ("__init__.py", "args.json"):
    shutil.copy(join(CLOUD, f), join(OUT, f))
from plyfile import PlyData, PlyElement
ply = PlyData.read(join(CLOUD, "data/root.primitives_pyr0.ply"))
v = ply["vertex"].data[sel]
el = PlyElement.describe(v, "vertex")
PlyData([el], text=False, byte_order="<").write(join(OUT, "data/root.primitives_pyr0.ply"))
shutil.copy(join(OUT, "data/root.primitives_pyr0.ply"), join(OUT, "root.primitives_pyr0.ply"))
json.dump({"indices": sel.tolist(), "sun": SUN.tolist(), "eps": EPS,
           "block": [r0, r1, c0, c1], "layout": LAYOUT,
           "coverage_min": float(cov_cam.min()), "coverage_med": float(np.median(cov_cam)),
           "n_probes": len(probes)},
          open(join(OUT, "manifest.json"), "w"), indent=1)
print(f"[extract] wrote {OUT} ({len(sel)} prims) + manifest.json")
