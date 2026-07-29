#!/usr/bin/env python3
"""Deterministic replay of the SAMPLER's optical depth on the mini-core scene.

Calls the fork's primitive_tracing with a recording callback that accumulates
seg_tau = sum_prims density_integral(seg_t0, seg_t1)*sigma_t per segment — i.e. exactly the tau
the flight sampler exponentiates — and compares against the exact chord tau per ray.
No Monte Carlo: any mismatch is a deterministic defect in the segment/stack machinery or kernel.

Run under with_pip_gabor.sh, CLOUD_DIR = mini pkg, VOLPRIM_DIR = the fixed fork.
Env: SG_ROWS/SG_COLS like "480 540" (probe block), SG_NPROBE grid step (default 10).
"""
import os, sys, json
from os.path import join
import numpy as np
import mitsuba as mi
mi.set_variant("cuda_ad_rgb")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gabor_bootstrap  # noqa: F401
import drjit as dr
import volprim.integrators.volprim_prb as vpprb
from volprim.integrators.common import Ellipsoid, PrimitiveID, primitive_tracing
from volprim.integrators.stack import alloc_stack
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
def quat_to_R(q, layout):
    x, y, z, w = (q[:, 0], q[:, 1], q[:, 2], q[:, 3]) if layout == "xyzw" else (q[:, 1], q[:, 2], q[:, 3], q[:, 0])
    n = np.sqrt(x*x + y*y + z*z + w*w); x, y, z, w = x/n, y/n, z/n, w/n
    R = np.empty((len(q), 3, 3))
    R[:, 0, 0] = 1 - 2*(y*y + z*z); R[:, 0, 1] = 2*(x*y - z*w);     R[:, 0, 2] = 2*(x*z + y*w)
    R[:, 1, 0] = 2*(x*y + z*w);     R[:, 1, 1] = 1 - 2*(x*x + z*z); R[:, 1, 2] = 2*(y*z - x*w)
    R[:, 2, 0] = 2*(x*z - y*w);     R[:, 2, 1] = 2*(y*z + x*w);     R[:, 2, 2] = 1 - 2*(x*x + y*y)
    return R
R = quat_to_R(QUAT, man["layout"])

def tau_exact_ray(o, w_dir):
    p = np.einsum("nij,nj->ni", np.transpose(R, (0, 2, 1)), (o[None] - CTR)) / SCL
    v = np.einsum("nij,j->ni", np.transpose(R, (0, 2, 1)), w_dir) / SCL
    vn = np.linalg.norm(v, axis=1); vhat = v / vn[:, None]
    B_ = (vhat * p).sum(1); C_ = (p * p).sum(1)
    disc = B_*B_ - (C_ - EXT*EXT)
    ok = disc > 0
    sq = np.sqrt(np.maximum(disc, 0))
    t0 = np.maximum((-B_ - sq) / vn, 0.0); t1 = (-B_ + sq) / vn
    ok &= t1 > t0
    ps = p + (t0 * vn)[:, None] * vhat
    L_ = (t1 - t0) * vn
    B = (vhat * ps).sum(1); C = (ps * ps).sum(1)
    s2 = 1.0 / np.sqrt(2.0)
    erf_term = 0.5 * (serf(s2 * (L_ + B)) - serf(s2 * B))
    D = (1.0 / vn) * np.exp(-0.5 * (C - B * B)) * erf_term / (2.0 * np.pi * SCL.prod(1))
    return np.where(ok, SIG * np.maximum(D, 0.0), 0.0).sum()

# probe rays through the sensor: block grid + thin-rim pixels
H, W = 600, 900
sensor = sc.sensors()[0]
tau_ex_img = np.load("results/campaign/nee_fair/minicore/tau_exact.npy")
r0, r1, c0, c1 = man["block"]
step = int(os.environ.get("SG_NPROBE", "10"))
pix = [(r, c) for r in range(r0 + 5, r1, step) for c in range(c0 + 5, c1, step)]
thin = np.argwhere((tau_ex_img > 0.05) & (tau_ex_img < 0.3))
rng = np.random.default_rng(3)
pix += [tuple(thin[i]) for i in rng.choice(len(thin), 24, replace=False)]
mid = np.argwhere((tau_ex_img > 1.0) & (tau_ex_img < 3.0))
pix += [tuple(mid[i]) for i in rng.choice(len(mid), 12, replace=False)]
M = len(pix)
pos = mi.Point2f(mi.Float([ (c + 0.5) / W for (r, c) in pix]), mi.Float([(r + 0.5) / H for (r, c) in pix]))
rays, _ = sensor.sample_ray(mi.Float([0.0]*M), mi.Float([0.5]*M), pos, mi.Point2f(0.5, 0.5), mi.Bool([True]*M))
def _col(v):
    a = np.array(v, dtype=np.float64)
    return np.repeat(a, M) if a.size == 1 else a       # drjit width-1 literals (ortho dir) -> broadcast
O = np.stack([_col(rays.o.x), _col(rays.o.y), _col(rays.o.z)], 1)
Dv = np.stack([_col(rays.d.x), _col(rays.d.y), _col(rays.d.z)], 1)
Dn = Dv / np.linalg.norm(Dv, axis=1, keepdims=True)

integ = sc.integrator()
kern = integ.kernel
SEGREC = []   # (seg_idx arrays) per segment call: t0, t1, stack_size, seg_tau  (evaluated mode)

@dr.syntax
def recorder(payload, primitives, ray, accum_t, seg_t0, seg_t1, active):
    (tau_acc,) = payload
    seg_tau = mi.Float(0.0)
    it = mi.UInt32(0)
    active1 = mi.Bool(active) & ~primitives.is_empty()
    while active1:
        prim_id = primitives.value(it, active1)
        ellipsoid = Ellipsoid.gather(prim_id.shape, prim_id.index, active1)
        dens = kern.density_integral(ray, ellipsoid, seg_t0, seg_t1, active1)
        seg_tau[active1] += dens * ellipsoid.opacity
        it += 1
        active1 &= (it < primitives.size())
    tau_acc = tau_acc + dr.select(active, seg_tau, 0.0)
    _ids = [np.array(primitives.value(mi.UInt32(_k), mi.Bool(active)).index) for _k in range(8)]
    SEGREC.append((np.array(seg_t0), np.array(seg_t1), np.array(seg_tau), np.array(active),
                   np.array(primitives.size()), np.array(accum_t), _ids))
    return mi.Bool(active) | mi.Bool(True), (tau_acc,)

# evaluated loops so the python-level recorder fires per segment
dr.set_flag(dr.JitFlag.SymbolicLoops, False)
dr.set_flag(dr.JitFlag.SymbolicCalls, False)

prims = alloc_stack(PrimitiveID, mi.UInt32, alloc_size=64)
smp = mi.load_dict({"type": "independent"}); smp.seed(0, M)
ray = mi.Ray3f(rays)
out = primitive_tracing(sc, smp, ray, prims, mi.UInt32(0),
                        callback=recorder, payload=(mi.Float([0.0] * M),),
                        active=mi.Bool([True] * M),
                        max_depth_primitive=-1, rr_depth_primitive=-1,
                        stochastic_selection_criteria="deterministic",
                        orientation_selection="deterministic")
tau_replay = np.array(out[3])  # returns primitives, si, weight, *payload

tau_ex = np.array([tau_exact_ray(O[i], Dn[i]) for i in range(M)])
print(f"\n{'pix':>12s} {'tau_exact':>10s} {'tau_replay':>11s} {'ratio':>7s}")
order = np.argsort(tau_ex)
for i in order:
    r, c = pix[i]
    flag = "  <-- MISMATCH" if abs(tau_replay[i] - tau_ex[i]) > 0.02 * max(tau_ex[i], 0.05) else ""
    print(f"({r:3d},{c:3d}) {tau_ex[i]:10.4f} {tau_replay[i]:11.4f} {tau_replay[i]/max(tau_ex[i],1e-9):7.4f}{flag}")
rat = tau_replay / np.maximum(tau_ex, 1e-9)
print(f"\nmedian ratio {np.median(rat):.4f}   worst {rat.max():.4f} / {rat.min():.4f}   segments recorded: {len(SEGREC)}")

# per-segment dump for the worst lane
Lbad = int(np.argmax(rat))
r, c = pix[Lbad]
print(f"\n=== segment dump, lane ({r},{c}): tau_ex {tau_ex[Lbad]:.4f}, replay {tau_replay[Lbad]:.4f} ===")
print(f"{'#':>3s} {'accum_t':>9s} {'t0':>8s} {'t1':>8s} {'abs0':>9s} {'abs1':>9s} {'seg_tau':>9s} {'nstk':>4s}  stack")
for j, rec in enumerate(SEGREC):
    t0, t1, st, act, nst, at, ids = rec
    def L(a):
        a = np.asarray(a); return a.flat[0] if a.size == 1 else a[Lbad]
    if not bool(L(act)):
        continue
    n = int(L(nst))
    stk = [int(L(ids[k])) for k in range(min(n, 8))]
    print(f"{j:3d} {L(at):9.4f} {L(t0):8.4f} {L(t1):8.4f} {L(at)+L(t0):9.4f} {L(at)+L(t1):9.4f} {L(st):9.4f} {n:4d}  {stk}")

# exact per-prim chord taus for that ray (top contributors)
pc = None
o_, w_ = O[Lbad], Dn[Lbad]
p_ = np.einsum("nij,nj->ni", np.transpose(R, (0, 2, 1)), (o_[None] - CTR)) / SCL
v_ = np.einsum("nij,j->ni", np.transpose(R, (0, 2, 1)), w_) / SCL
vn_ = np.linalg.norm(v_, axis=1); vh_ = v_ / vn_[:, None]
B2 = (vh_ * p_).sum(1); C2 = (p_ * p_).sum(1)
disc2 = B2*B2 - (C2 - EXT*EXT); ok2 = disc2 > 0
sq2 = np.sqrt(np.maximum(disc2, 0))
tt0 = np.maximum((-B2 - sq2) / vn_, 0.0); tt1 = (-B2 + sq2) / vn_
ok2 &= tt1 > tt0
ps2 = p_ + (tt0 * vn_)[:, None] * vh_
LL = (tt1 - tt0) * vn_
B3 = (vh_ * ps2).sum(1); C3 = (ps2 * ps2).sum(1)
s2c = 1.0/np.sqrt(2.0)
erf2 = 0.5 * (serf(s2c * (LL + B3)) - serf(s2c * B3))
D2 = (1.0/vn_) * np.exp(-0.5*(C3 - B3*B3)) * erf2 / (2.0*np.pi*SCL.prod(1))
tpp = np.where(ok2, SIG * np.maximum(D2, 0.0), 0.0)
top = np.argsort(tpp)[::-1][:8]
print("\nexact per-prim chord tau (top 8):")
for i in top:
    print(f"  prim {i:3d}: tau {tpp[i]:.4f}   window [{tt0[i]:.4f},{tt1[i]:.4f}]  scales {SCL[i].round(4)}")

np.savez("results/campaign/nee_fair/minicore/tau_replay.npz",
         pix=np.array(pix), tau_ex=tau_ex, tau_replay=tau_replay)
