"""Bootstrap the LATEST Gabor Fields volprim against whatever Mitsuba/drjit is importable.

Each shim is applied ONLY if the running Mitsuba/drjit lacks the native API, so on the official
mitsuba 3.8.0 + drjit 1.3.1 release (which merged the ellipsoids work) almost nothing is shimmed.
Every shim is a pure API translation or targets a feature irrelevant to scattering media; none touch
the volprim_prb NEE/analog sample() math. Validity is self-checked downstream by the furnace analog
control returning exactly 1.0.

Shims (guarded):
  1. is_ellipsoids()  ==  (shape_type() == +ShapeType.Ellipsoids)   [exact semantic; native on 3.8.0]
  2. dr.freeze        ==  no-op decorator (only decorates optimizers.py, never the render path; native
     on drjit >= 1.1)
  3. Ray3f.mask       ==  write-only no-op (ellipsoid orientation-VIEW mask; a Gabor-paper multi-view
     feature NOT in the official release; only assigned in Python, never read — its sole consumer is
     the C++ ray_intersect, which without the field intersects the full ellipsoid. Irrelevant to a
     scattering medium, whose ellipsoids are view-independent. Combined with deterministic selection
     [applied in the harness] this is the complete, unbiased trace.)
  4. dr.quat_to_matrix new (target_type, quat) signature -> old (quat, size)   [native on drjit 1.3.1]

Usage: import this FIRST (after putting the Gabor volprim checkout on PYTHONPATH), then import volprim.*
"""
import mitsuba as mi
import drjit as dr

if not mi.variant():
    mi.set_variant("cuda_ad_rgb")

_applied = []

# --- shim 1: is_ellipsoids() on Shape + ShapePtr ---
_is_ell = lambda self: self.shape_type() == +mi.ShapeType.Ellipsoids
for _cls in (mi.Shape, mi.ShapePtr):
    if not hasattr(_cls, "is_ellipsoids"):
        _cls.is_ellipsoids = _is_ell
        _applied.append("is_ellipsoids:" + _cls.__name__)

# --- shim 2: dr.freeze no-op ---
if not hasattr(dr, "freeze"):
    def _freeze(*a, **k):
        if len(a) == 1 and callable(a[0]) and not k:
            return a[0]          # bare @dr.freeze
        return lambda f: f       # @dr.freeze(...)
    dr.freeze = _freeze
    _applied.append("dr.freeze")

# --- shim 3: Ray3f.mask write-only no-op (multi-view feature; irrelevant to scattering media) ---
if not hasattr(mi.Ray3f, "mask"):
    mi.Ray3f.mask = property(lambda self: mi.UInt32(255), lambda self, v: None)
    _applied.append("Ray3f.mask")

# --- shim 4: dr.quat_to_matrix signature translation — only if the native one rejects (type, quat) ---
def _needs_q2m_shim():
    try:
        dr.quat_to_matrix(mi.Matrix3f, mi.Quaternion4f(0.0, 0.0, 0.0, 1.0))
        return False
    except Exception:
        return True
if _needs_q2m_shim():
    _orig_q2m = dr.quat_to_matrix
    def _q2m(*args, **kw):
        if len(args) == 2 and isinstance(args[0], type):     # new form: (Matrix3f|Matrix4f, quat)
            target, q = args
            return _orig_q2m(q, size=3 if "3" in target.__name__ else 4)
        return _orig_q2m(*args, **kw)                         # old form: (quat, size=...)
    dr.quat_to_matrix = _q2m
    _applied.append("quat_to_matrix")

# --- shim 5: TraversalCallback.put_parameter -> put (3.8.0 renamed it). Pure rename in the
# parameter-EXPOSURE path (used to scale sigma_t/albedo before render); does NOT touch render math. ---
if hasattr(mi, "TraversalCallback") and not hasattr(mi.TraversalCallback, "put_parameter") \
        and hasattr(mi.TraversalCallback, "put"):
    mi.TraversalCallback.put_parameter = mi.TraversalCallback.put
    _applied.append("put_parameter->put")

print(f"[gabor_bootstrap] mitsuba {getattr(mi,'__version__','?')} drjit {getattr(dr,'__version__','?')} "
      f"variant={mi.variant()} | shims applied: {_applied or 'NONE (fully native)'}")
