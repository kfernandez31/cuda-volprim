# tools/refs/probe_maxt.py — E1: print ds.dist/maxt for a constant-emitter NEE shadow ray (chain
# scene); E2: from a point inside one ellipsoid, what does ray_intersect report along +z — the
# current primitive's EXIT or the next primitive's ENTRY? (fixes oracle enumeration variant V1 vs V2)
import os, sys
import numpy as np
import mitsuba as mi
mi.set_variant("cuda_ad_rgb")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gabor_bootstrap  # noqa: F401
import drjit as dr

N, D, SIG = 4, 2.0, 6.0
ctr = np.zeros((N, 3), np.float32)
ctr[:, 2] = (np.arange(N) - (N - 1) / 2.0) * D          # centers z = -3,-1,+1,+3
sc = mi.load_dict({"type": "scene",
    "p": {"type": "ellipsoids",
          "centers": mi.TensorXf(ctr.ravel(), shape=(N, 3)),
          "scales": mi.TensorXf(np.ones((N, 3), np.float32).ravel(), shape=(N, 3)),
          "quaternions": mi.TensorXf(np.tile([0, 0, 0, 1], (N, 1)).astype(np.float32).ravel(), shape=(N, 4)),
          "opacities": mi.TensorXf(np.full((N, 1), SIG, np.float32).ravel(), shape=(N, 1)),
          "albedo": mi.TensorXf(np.ones((N, 3), np.float32).ravel(), shape=(N, 3)),
          "extent": 3.0},
    "e": {"type": "constant", "radiance": {"type": "uniform", "value": 1.0}}})

# --- E1: shadow-ray maxt for the constant emitter ---
em = sc.environment()
ref = dr.zeros(mi.Interaction3f)
ref.p = mi.Point3f(0, 0, -3.0)                           # a vertex inside the chain (prim 0 centre)
ds, w = em.sample_direction(ref, mi.Point2f(0.3, 0.7), True)
ray2 = ref.spawn_ray_to(ds.p)
print("E1: ds.dist =", ds.dist, " ray2.maxt =", ray2.maxt, " scene bbox =", sc.bbox())

# --- E2: entry-only or exit hits? ---
si = sc.ray_intersect(mi.Ray3f(mi.Point3f(0, 0, -3.0), mi.Vector3f(0, 0, 1)))
print("E2: hit t =", si.t, " prim_index =", si.prim_index)
# from inside prim 0 (support sphere radius 3 around z=-3, exit at t=3):
#   prim 1 (z=-1) entry would be t = 2 - 3 = -1 -> already inside prim 1 too!
# So also probe from OUTSIDE all support spheres for a clean entry distance:
si2 = sc.ray_intersect(mi.Ray3f(mi.Point3f(0, 0, -10.0), mi.Vector3f(0, 0, 1)))
print("E2b (from z=-10, first entry should be prim0 at t=4): t =", si2.t, " prim =", si2.prim_index)
# and from a point inside prims 0+1 but before prim 2's entry:
si3 = sc.ray_intersect(mi.Ray3f(mi.Point3f(0, 0, -2.5), mi.Vector3f(0, 0, 1)))
print("E2c (from z=-2.5, inside prims 0,1; prim2 entry at t=0.5, prim0 exit t=2.5): t =", si3.t,
      " prim =", si3.prim_index)
