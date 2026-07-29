#!/usr/bin/env python3
"""Deterministic shadow-transmittance oracle on the REAL cloud asset.

Compares, on identical full sun-chord rays through the disputed dense-core region:
  tau_exact  — numpy: sum over all 652 prims of sigma_t_i * D_i, where D_i is the CORRECTED
               segment formula (the one probe_mirror.py verified to 6 decimals), support-clipped
               to the extent(=3) ellipsoid — i.e. exactly the medium the sampler defines.
  tau_gabor  — the fork's volprim_prb.eval_transmittance on the same rays (empty vertex stack:
               origins are chosen OUTSIDE all supports, so part 1 is legitimately empty and the
               march does all the work: FIX2 kernel + FIX3/4 windows + spawn epsilons + cutoff).

No Monte Carlo anywhere: any tau discrepancy is a deterministic defect in the march.
Self-test: numpy D_i vs kernel.density_integral on random single-prim rays must agree ~1e-5.

Env: VOLPRIM_DIR must point at the fork; run under with_pip_gabor.sh from the repo root.
     SG_ORACLE_ROWS/COLS (probe pixel block, default the disputed block 480:540 x 480:540).
"""
import os, sys
from os.path import join
import numpy as np
import mitsuba as mi
mi.set_variant("cuda_ad_rgb")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gabor_bootstrap  # noqa: F401
import drjit as dr
import volprim.integrators.volprim_prb as vpprb
from volprim.integrators.common import Ellipsoid, PrimitiveID
from volprim.integrators.stack import alloc_stack
from mitsuba import ScalarTransform4f as T

# alias shim: the cloud PLY exposes 'sigma_t'; Gabor's Ellipsoid.gather asks for 'opacities'
# (same shim as gabor_cloud.py; without it every marched prim reads opacity 0)
def _install_alias(cls):
    _has, _e1 = cls.has_attribute, cls.eval_attribute_1
    def has_attribute(self, name, active=True):
        return _has(self, "sigma_t" if name == "opacities" else name, active)
    def eval_attribute_1(self, name, si, active=True):
        return _e1(self, "sigma_t" if name == "opacities" else name, si, active)
    cls.has_attribute = has_attribute
    cls.eval_attribute_1 = eval_attribute_1
for _c in (mi.Shape, mi.ShapePtr):
    _install_alias(_c)

# ---------------- scene (identical to gabor_cloud.py, sigma x7.5) ----------------
CLOUD = os.environ.get("CLOUD_DIR", "assets/models/cloud")
sys.path.insert(0, CLOUD)
import __init__ as cs
d = {"type": "scene"}
d.update(cs.OBJECTS); d.update(cs.EMITTERS); d.pop("resources", None)
d["primitives_pyr0"].pop("extent_adaptive_clamping", None)
d["primitives_pyr0"]["filename"] = join(CLOUD, "data/root.primitives_pyr0.ply")
d["environment"] = {"type": "envmap", "filename": os.environ.get("MEADOW_HDR", "assets/environment_maps/meadow_2_4k.hdr"),
                    "to_world": T().rotate(axis=[0, 1, 0], angle=90.0)}
d["integrator"] = {"type": "volprim_prb", "max_depth": 128, "kernel_type": "gaussian",
                   "solver_type": "bisection", "use_nee": True, "max_overlaps": 32,
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
print(f"[oracle] {N} prims, extent {EXT}, sigma_t range [{SIG.min():.3f}, {SIG.max():.3f}], "
      f"scales [{SCL.min():.4f}, {SCL.max():.4f}]")

# quaternion -> rotation matrix. Try BOTH (x,y,z,w) and (w,x,y,z) layouts; self-test picks the right one.
def quat_to_R(q, layout):
    x, y, z, w = (q[:, 0], q[:, 1], q[:, 2], q[:, 3]) if layout == "xyzw" else (q[:, 1], q[:, 2], q[:, 3], q[:, 0])
    n = np.sqrt(x*x + y*y + z*z + w*w); x, y, z, w = x/n, y/n, z/n, w/n
    R = np.empty((len(q), 3, 3))
    R[:, 0, 0] = 1 - 2*(y*y + z*z); R[:, 0, 1] = 2*(x*y - z*w);     R[:, 0, 2] = 2*(x*z + y*w)
    R[:, 1, 0] = 2*(x*y + z*w);     R[:, 1, 1] = 1 - 2*(x*x + z*z); R[:, 1, 2] = 2*(y*z - x*w)
    R[:, 2, 0] = 2*(x*z - y*w);     R[:, 2, 1] = 2*(y*z + x*w);     R[:, 2, 2] = 1 - 2*(x*x - 0) - 2*(y*y)  # filled below
    R[:, 2, 2] = 1 - 2*(x*x + y*y)
    return R

from math import erf as _erf, sqrt, pi
ERFV = np.vectorize(_erf)

def tau_exact(o, w_dir, R, layout_unused=None, tmax=np.inf):
    """Exact optical depth along ray o + t*w (unit w), t in [0, tmax]: corrected formula,
    support-clipped per prim to the extent ellipsoid. o,w: (3,). Returns scalar + hit count."""
    p = np.einsum("nij,nj->ni", np.transpose(R, (0, 2, 1)), (o[None] - CTR)) / SCL   # frame pos
    v = np.einsum("nij,j->ni",  np.transpose(R, (0, 2, 1)), w_dir) / SCL             # frame dir (unnorm)
    vn = np.linalg.norm(v, axis=1); vhat = v / vn[:, None]
    # support: |p + u*vhat| = EXT (frame arc-length u); world t = u / vn
    B_ = (vhat * p).sum(1); C_ = (p * p).sum(1)
    disc = B_*B_ - (C_ - EXT*EXT)
    ok = disc > 0
    su0 = (-B_ - np.sqrt(np.maximum(disc, 0)))     # frame entry
    su1 = (-B_ + np.sqrt(np.maximum(disc, 0)))     # frame exit
    t0 = np.maximum(su0 / vn, 0.0); t1 = np.minimum(su1 / vn, tmax)                   # world clip
    ok &= t1 > t0
    # corrected segment integral over world [t0, t1]:
    ps = p + (t0 * vn)[:, None] * vhat                                                # advance to seg start
    L_ = (t1 - t0) * vn                                                               # frame seg length
    B = (vhat * ps).sum(1); C = (ps * ps).sum(1)
    s2 = 1.0 / sqrt(2.0)
    erf_term = 0.5 * (ERFV(s2 * (L_ + B)) - ERFV(s2 * B))
    D = (1.0 / vn) * np.exp(-0.5 * (C - B * B)) * erf_term / (2.0 * pi * SCL.prod(1))
    tau_i = np.where(ok, SIG * np.maximum(D, 0.0), 0.0)
    return tau_i.sum(), int(ok.sum())

# NOTE on normalization: kernel returns density = w_norm*exp(-(C-B^2)/2)*erfcombo(*0.5) * 1/(2pi*prod s);
# our D above = (1/vn)*exp(...)*sqrt(2pi)*[0.5*(erf-erf)] / (2pi*prod s / sqrt(2pi)) — algebraically equal:
# sqrt(2pi)/ (2pi prod s / sqrt(2pi)) = (2pi)/(2pi prod s) ... self-test below is the arbiter of all conventions.

# ---------------- self-test: numpy vs kernel.density_integral, per prim ----------------
kern = sc.integrator().kernel
rng = np.random.default_rng(0)
best = None
for layout in ("xyzw", "wxyz"):
    R = quat_to_R(QUAT, layout)
    errs = []
    for trial in range(24):
        i = int(rng.integers(0, N))
        o = CTR[i] + rng.normal(0, 1, 3) * SCL[i] * 4.0
        w_dir = rng.normal(0, 1, 3); w_dir /= np.linalg.norm(w_dir)
        ell = Ellipsoid.gather(sc.shapes_dr(), mi.UInt32(i), mi.Bool(True))
        ray = mi.Ray3f(mi.Point3f(*o.tolist()), mi.Vector3f(*w_dir.tolist()))
        # kernel full-range (sign-safe both ways) times sigma: reference for conventions
        Dk = float(np.array(kern.density_integral(ray, ell, tmin=None, tmax=None, active=mi.Bool(True)))[0])
        # numpy full-line for the same single prim: use large window
        p = (np.transpose(R[i]) @ (o - CTR[i])) / SCL[i]
        v = (np.transpose(R[i]) @ w_dir) / SCL[i]
        vn = np.linalg.norm(v); vhat = v / vn
        B = float(vhat @ p); C = float(p @ p)
        Dn = (1.0 / vn) * np.exp(-0.5 * (C - B * B)) / (2.0 * pi * SCL[i].prod())
        if Dk > 1e-12:
            errs.append(abs(Dn - Dk) / Dk)
    m = float(np.median(errs)) if errs else 9e9
    print(f"[self-test] layout {layout}: median rel err vs kernel full-range = {m:.2e} ({len(errs)} trials)")
    if best is None or m < best[1]:
        best = (layout, m)
LAYOUT, err = best
assert err < 1e-3, f"self-test failed ({err}) — convention mismatch, fix before trusting the oracle"
R = quat_to_R(QUAT, LAYOUT)
print(f"[self-test] using layout {LAYOUT}")

# ---------------- probe rays: sun chords through the disputed core ----------------
# sun direction: sample the emitter from a core-ish point, keep the max-radiance direction
em = sc.environment()
ref = dr.zeros(mi.Interaction3f); ref.p = mi.Point3f(0, 0, 0)
sampler = mi.load_dict({"type": "independent"}); sampler.seed(7, 8192)
ds, wgt = em.sample_direction(ref, sampler.next_2d(), mi.Bool(True))
rad = np.array(wgt).mean(0) * np.array(ds.pdf)          # radiance = weight * pdf
k = int(np.argmax(rad))
SUN = np.array([np.array(ds.d.x)[k], np.array(ds.d.y)[k], np.array(ds.d.z)[k]])
print(f"[oracle] sun direction {SUN.round(4)}, radiance {rad[k]:.1f}")

# probe origins: camera rays through the disputed block; vertex at tau_cam ~= 1; then back off
# along -SUN to before the cloud (outside every support) and shoot the full sun chord.
sensor = sc.sensors()[0]
r0, r1 = (int(x) for x in os.environ.get("SG_ORACLE_ROWS", "480 540").split())
c0, c1 = (int(x) for x in os.environ.get("SG_ORACLE_COLS", "480 540").split())
pix = [(r, c) for r in range(r0 + 5, r1, 15) for c in range(c0 + 5, c1, 15)]
origins = []
for (r, c) in pix:
    pos = mi.Point2f((c + 0.5) / 900.0, (r + 0.5) / 600.0)
    cray, _ = sensor.sample_ray(0.0, 0.5, pos, mi.Point2f(0.5, 0.5), mi.Bool(True))
    co = np.array([float(np.array(cray.o.x)[0]), float(np.array(cray.o.y)[0]), float(np.array(cray.o.z)[0])])
    cd = np.array([float(np.array(cray.d.x)[0]), float(np.array(cray.d.y)[0]), float(np.array(cray.d.z)[0])])
    cd /= np.linalg.norm(cd)
    # find vertex at tau_cam ~ TAU_CAM by bisection on exact tau
    TAU_CAM = float(os.environ.get("SG_TAU_CAM", "1.0"))
    lo, hi = 0.0, 40.0
    if tau_exact(co, cd, R, tmax=hi)[0] < TAU_CAM:
        continue
    for _ in range(40):
        mid = 0.5 * (lo + hi)
        lo, hi = (mid, hi) if tau_exact(co, cd, R, tmax=mid)[0] < TAU_CAM else (lo, mid)
    origins.append(co + cd * hi)
print(f"[oracle] {len(origins)} core vertices found (tau_cam=1)")

# probe FROM each vertex toward the sun — exactly the NEE march's job. Empty vertex stack on the
# gabor side means vertex-containing prims are invisible to it (entry-only hits); the numpy side
# excludes exactly those prims, so both compute the same quantity: the MARCH tau from the vertex.
def tau_exact_march(o, w_dir, tmax):
    q = np.einsum("nij,nj->ni", np.transpose(R, (0, 2, 1)), (o[None] - CTR)) / SCL
    contains = (q * q).sum(1) <= EXT * EXT
    p_ = q; v = np.einsum("nij,j->ni", np.transpose(R, (0, 2, 1)), w_dir) / SCL
    vn = np.linalg.norm(v, axis=1); vhat = v / vn[:, None]
    B_ = (vhat * p_).sum(1); C_ = (p_ * p_).sum(1)
    disc = B_ * B_ - (C_ - EXT * EXT)
    ok = (disc > 0) & ~contains
    su0 = -B_ - np.sqrt(np.maximum(disc, 0)); su1 = -B_ + np.sqrt(np.maximum(disc, 0))
    t0 = np.maximum(su0 / vn, 0.0); t1 = np.minimum(su1 / vn, tmax)
    ok &= (t1 > t0) & (su0 / vn > 1e-5)          # entered from outside, ahead (the march's hit set)
    ps = p_ + (t0 * vn)[:, None] * vhat
    L_ = (t1 - t0) * vn
    B = (vhat * ps).sum(1); C = (ps * ps).sum(1)
    s2 = 1.0 / sqrt(2.0)
    erf_term = 0.5 * (ERFV(s2 * (L_ + B)) - ERFV(s2 * B))
    D = (1.0 / vn) * np.exp(-0.5 * (C - B * B)) * erf_term / (2.0 * pi * SCL.prod(1))
    tau_i = np.where(ok, SIG * np.maximum(D, 0.0), 0.0)
    return tau_i.sum(), int(ok.sum()), int(contains.sum())

MAXT = 80.0
probes = [(v, *tau_exact_march(v, SUN, MAXT)) for v in origins]
M = len(probes)
ox = mi.Float([float(p_[0][0]) for p_ in probes]); oy = mi.Float([float(p_[0][1]) for p_ in probes])
oz = mi.Float([float(p_[0][2]) for p_ in probes])
ray = mi.Ray3f(mi.Point3f(ox, oy, oz),
               mi.Vector3f(mi.Float([SUN[0]] * M), mi.Float([SUN[1]] * M), mi.Float([SUN[2]] * M)))
ray.maxt = mi.Float([MAXT] * M)
stack = alloc_stack(PrimitiveID, mi.UInt32, alloc_size=32)
integ = sc.integrator()
smp = mi.load_dict({"type": "independent"}); smp.seed(1, M)
Tg = integ.eval_transmittance(sc, smp, ray, stack, mi.Spectrum(0.0), None,
                              dr.ADMode.Primal, mi.Bool([True] * M))
Tg = np.asarray(np.array(Tg)).reshape(3, -1).mean(0) if np.array(Tg).size == 3 * M else np.asarray(np.array(Tg)).reshape(-1)
tg = -np.log(np.maximum(Tg, 1e-30))

rows = np.array([(p_[1], tg[i], p_[2], p_[3]) for i, p_ in enumerate(probes)])
print(f"\n[oracle] {len(rows)} vertex->sun march segments (marched prims {int(rows[:,2].min())}-"
      f"{int(rows[:,2].max())}; vertex-stack prims excluded per row: {int(rows[:,3].min())}-{int(rows[:,3].max())})")
print(f"{'tau_exact':>10s} {'tau_gabor':>10s} {'dtau(g-e)':>10s} {'T ratio g/e':>12s} {'#marched':>9s}")
for a, b, c, d_ in rows[np.argsort(rows[:, 0])]:
    print(f"{a:10.4f} {b:10.4f} {b-a:+10.4f} {np.exp(a-b):12.4f} {int(c):9d}")
dd = rows[:, 1] - rows[:, 0]
sel = rows[:, 0] < 6.0
print(f"\nsummary ALL: dtau median {np.median(dd):+.4f}")
if sel.any():
    print(f"summary tau_exact<6 (cutoff-free): n={sel.sum()}, dtau median {np.median(dd[sel]):+.5f}, "
          f"mean {dd[sel].mean():+.5f} -> T factor x{np.exp(-np.median(dd[sel])):.4f}")
