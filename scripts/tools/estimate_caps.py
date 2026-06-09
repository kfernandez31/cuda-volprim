#!/usr/bin/env python3
"""Offline estimator for the renderer's two compile-time, asset-dependent caps.

`MAX_ACTIVE_PRIMS` and `HIT_BUFFER_CAPACITY` (device/core/constants.cuh) are sized
at compile time for the target asset. A denser or differently-shaped scene can
exceed them; on overflow the renderer is *safe and detectable* (it drops the excess
and increments a device counter) but biased (under-absorption -> too bright). This
tool predicts the right caps from an asset's geometry alone -- no GPU, no recompile --
so the caps can be set correctly *before* building for a new asset.

It reproduces the renderer's exact geometric criterion: every primitive is bounded
by its 3-sigma ellipsoid (GAUSSIAN_EXTENT_F = 3 in common/utils/math.h), which is
both what OptiX traverses (localToWorld inflates the unit sphere by 3*scale) and
what the active-set test uses (point_inside_bvh_bound: ||R^T (x - c) / scale||^2 <= 9).

Two caps, two geometric questions:
  * MAX_ACTIVE_PRIMS  <- max number of 3-sigma ellipsoids that simultaneously
                         CONTAIN a point (point overlap).
  * HIT_BUFFER_CAPACITY <- max number of 3-sigma ellipsoids a single ray CROSSES
                         (ray-ellipsoid entries).

The estimate is a slight over-estimate (the 3-sigma geometric shell is wider than the
density-significant region), hence SAFE: a cap >= the reported max never overflows.

PLY convention (matches src/.../io/ply.cpp): per-vertex floats include
  x,y,z (center), rot_0..3 (quaternion, w x y z), scale_0..2 (LOG scale; linear = exp).

Usage:
  python3 scripts/tools/estimate_caps.py assets/models/cloud/root.primitives_pyr0.ply
  python3 scripts/tools/estimate_caps.py A.ply B.ply --margin 1.25
  python3 scripts/tools/estimate_caps.py *.ply --csv caps.csv
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

import numpy as np

# Matches common::math::GAUSSIAN_EXTENT_F (= 3 sigma covers 99.7% of mass), the factor
# baked into both the BVH localToWorld transform and point_inside_bvh_bound.
GAUSSIAN_EXTENT = 3.0

# Current compile-time caps (device/core/constants.cuh) -- for the overflow warning.
CURRENT_MAX_ACTIVE_PRIMS = 128
CURRENT_HIT_BUFFER_CAPACITY = 128


# --------------------------------------------------------------------------- #
# PLY parsing (binary_little_endian, all-float vertex records)
# --------------------------------------------------------------------------- #

_PLY_FLOAT_SIZES = {"float": 4, "float32": 4, "double": 8, "float64": 8}


@dataclass
class Asset:
    name: str
    center: np.ndarray   # (N, 3) float32
    scale: np.ndarray    # (N, 3) float32, LINEAR sigma (exp of log-scale)
    quat: np.ndarray     # (N, 4) float32, normalized (w, x, y, z)

    @property
    def count(self) -> int:
        return self.center.shape[0]


def load_ply(path: Path) -> Asset:
    """Parse a binary-LE PLY into per-primitive center/scale/quat, matching ply.cpp."""
    with open(path, "rb") as f:
        if f.readline().strip() != b"ply":
            raise ValueError(f"{path}: not a PLY file")
        fmt = f.readline().strip()
        if fmt != b"format binary_little_endian 1.0":
            raise ValueError(f"{path}: unsupported PLY format {fmt!r} "
                             "(only binary_little_endian 1.0)")
        props: list[tuple[str, str]] = []  # (type, name) in file order
        n_vertices = 0
        in_vertex = False
        while True:
            line = f.readline()
            if not line:
                raise ValueError(f"{path}: unexpected EOF in header")
            tok = line.split()
            if tok[0] == b"element":
                in_vertex = tok[1] == b"vertex"
                if in_vertex:
                    n_vertices = int(tok[2])
            elif tok[0] == b"property" and in_vertex:
                if tok[1] == b"list":
                    raise ValueError(f"{path}: list property in vertex element unsupported")
                props.append((tok[1].decode(), tok[2].decode()))
            elif tok[0] == b"end_header":
                break

        names = [n for _, n in props]
        sizes = [_PLY_FLOAT_SIZES.get(t) for t, _ in props]
        if any(s is None for s in sizes):
            bad = [t for (t, _), s in zip(props, sizes) if s is None]
            raise ValueError(f"{path}: non-float vertex property type(s) {bad}")
        stride = sum(sizes)  # type: ignore[arg-type]

        required = ["x", "y", "z", "rot_0", "rot_1", "rot_2", "rot_3",
                    "scale_0", "scale_1", "scale_2"]
        missing = [r for r in required if r not in names]
        if missing:
            raise ValueError(f"{path}: missing required vertex propert(ies) {missing}")

        raw = f.read(stride * n_vertices)
        if len(raw) < stride * n_vertices:
            raise ValueError(f"{path}: truncated vertex data")

    # All-float fast path (the DSYG export is uniformly float32); fall back to a
    # generic struct unpack only if some property is double.
    if all(s == 4 for s in sizes):
        table = np.frombuffer(raw, dtype="<f4").reshape(n_vertices, len(props))
    else:
        col_fmt = "<" + "".join("f" if s == 4 else "d" for s in sizes)
        unpack = struct.Struct(col_fmt).unpack_from
        table = np.empty((n_vertices, len(props)), dtype=np.float64)
        for i in range(n_vertices):
            table[i] = unpack(raw, i * stride)

    col = {n: table[:, names.index(n)].astype(np.float32) for n in required}
    center = np.stack([col["x"], col["y"], col["z"]], axis=1)
    log_scale = np.stack([col["scale_0"], col["scale_1"], col["scale_2"]], axis=1)
    scale = np.exp(log_scale)  # ply.cpp: scale = expf(scale_i)
    quat = np.stack([col["rot_0"], col["rot_1"], col["rot_2"], col["rot_3"]], axis=1)
    quat /= np.linalg.norm(quat, axis=1, keepdims=True)  # UnitQuaternion::from normalizes
    return Asset(name=path.stem, center=center, scale=scale, quat=quat)


# --------------------------------------------------------------------------- #
# Geometry
# --------------------------------------------------------------------------- #

def quat_to_world_to_local(quat: np.ndarray) -> np.ndarray:
    """Per-primitive world->local rotation matrices R^T from (w,x,y,z) quaternions.

    The loader stores the CONJUGATE for world->local; transform_pos_local applies it.
    The forward (local->world) matrix R for unit q=(w,x,y,z) is the standard one; the
    world->local matrix used by the criterion is R^T. Returns (N, 3, 3).
    """
    w, x, y, z = quat[:, 0], quat[:, 1], quat[:, 2], quat[:, 3]
    # Forward rotation R (local->world).
    r00 = 1 - 2 * (y * y + z * z)
    r01 = 2 * (x * y - w * z)
    r02 = 2 * (x * z + w * y)
    r10 = 2 * (x * y + w * z)
    r11 = 1 - 2 * (x * x + z * z)
    r12 = 2 * (y * z - w * x)
    r20 = 2 * (x * z - w * y)
    r21 = 2 * (y * z + w * x)
    r22 = 1 - 2 * (x * x + y * y)
    R = np.stack([r00, r01, r02, r10, r11, r12, r20, r21, r22], axis=1)
    R = R.reshape(-1, 3, 3).astype(np.float32)
    return np.transpose(R, (0, 2, 1))  # R^T (world->local)


def point_overlap_counts(asset: Asset, queries: np.ndarray, rt: np.ndarray,
                         chunk: int = 256) -> np.ndarray:
    """For each query point, count primitives whose 3-sigma ellipsoid contains it.

    Criterion (exactly point_inside_bvh_bound): ||R^T (p - c) / scale||^2 <= 3^2.
    """
    r2 = GAUSSIAN_EXTENT * GAUSSIAN_EXTENT
    inv_scale = (1.0 / asset.scale).astype(np.float32)  # (N, 3)
    out = np.empty(queries.shape[0], dtype=np.int32)
    for lo in range(0, queries.shape[0], chunk):
        q = queries[lo:lo + chunk]                       # (Q, 3)
        diff = q[:, None, :] - asset.center[None, :, :]  # (Q, N, 3) world
        # local = R^T @ diff  (per primitive), then * inv_scale
        local = np.einsum("nij,qnj->qni", rt, diff, optimize=True)
        local *= inv_scale[None, :, :]
        d2 = np.einsum("qni,qni->qn", local, local, optimize=True)
        out[lo:lo + chunk] = np.count_nonzero(d2 <= r2, axis=1)
    return out


def ray_cross_counts(asset: Asset, origins: np.ndarray, dirs: np.ndarray,
                     rt: np.ndarray, chunk: int = 256) -> np.ndarray:
    """For each line, count primitives whose 3-sigma ellipsoid it stabs.

    Transform the line into each primitive's whitened space (where the 3-sigma shell is
    the radius-3 sphere) and test perpendicular distance from the origin to the line:
    ||o_w||^2 - (o_w . dhat_w)^2 <= 3^2. Infinite lines are used (conservative: a finite
    camera ray crosses no more than its supporting line), keeping the estimate safe.
    """
    r2 = GAUSSIAN_EXTENT * GAUSSIAN_EXTENT
    inv_scale = (1.0 / asset.scale).astype(np.float32)
    out = np.empty(origins.shape[0], dtype=np.int32)
    for lo in range(0, origins.shape[0], chunk):
        o = origins[lo:lo + chunk]                       # (L, 3)
        d = dirs[lo:lo + chunk]                          # (L, 3)
        diff = o[:, None, :] - asset.center[None, :, :]  # (L, N, 3)
        o_w = np.einsum("nij,lnj->lni", rt, diff, optimize=True) * inv_scale[None, :, :]
        d_w = np.einsum("nij,lj->lni", rt, d, optimize=True) * inv_scale[None, :, :]
        d_len2 = np.einsum("lni,lni->ln", d_w, d_w, optimize=True)
        o_len2 = np.einsum("lni,lni->ln", o_w, o_w, optimize=True)
        od = np.einsum("lni,lni->ln", o_w, d_w, optimize=True)
        # perp^2 = ||o_w||^2 - (o_w . d_w)^2 / ||d_w||^2
        perp2 = o_len2 - (od * od) / np.maximum(d_len2, 1e-30)
        out[lo:lo + chunk] = np.count_nonzero(perp2 <= r2, axis=1)
    return out


# --------------------------------------------------------------------------- #
# Sampling
# --------------------------------------------------------------------------- #

def scene_aabb(asset: Asset) -> tuple[np.ndarray, np.ndarray]:
    """AABB of the union of 3-sigma ellipsoids (centers padded by 3 * max axis scale)."""
    ext = GAUSSIAN_EXTENT * asset.scale.max(axis=1, keepdims=True)  # (N,1) worst axis
    lo = (asset.center - ext).min(axis=0)
    hi = (asset.center + ext).max(axis=0)
    return lo.astype(np.float32), hi.astype(np.float32)


def sample_points(asset: Asset, n_random: int, rng: np.random.Generator) -> np.ndarray:
    """Query points = all primitive centers (densest overlap clusters near a center)
    plus uniform random points in the AABB (catches off-center stacks)."""
    lo, hi = scene_aabb(asset)
    rand = rng.uniform(lo, hi, size=(n_random, 3)).astype(np.float32)
    return np.concatenate([asset.center, rand], axis=0)


def sample_rays(asset: Asset, n_rays: int,
                rng: np.random.Generator) -> tuple[np.ndarray, np.ndarray]:
    """Lines through two random points in the AABB -- concentrates lines in the
    populated volume, so the worst-case chord is well sampled."""
    lo, hi = scene_aabb(asset)
    a = rng.uniform(lo, hi, size=(n_rays, 3)).astype(np.float32)
    b = rng.uniform(lo, hi, size=(n_rays, 3)).astype(np.float32)
    d = b - a
    n = np.linalg.norm(d, axis=1, keepdims=True)
    d = np.divide(d, n, out=np.zeros_like(d), where=n > 1e-12)
    return a, d


# --------------------------------------------------------------------------- #
# Reporting
# --------------------------------------------------------------------------- #

def stats(counts: np.ndarray) -> dict:
    return {
        "max": int(counts.max()),
        "p99": int(np.percentile(counts, 99)),
        "p50": int(np.percentile(counts, 50)),
        "mean": float(counts.mean()),
    }


def suggest_cap(max_count: int, margin: float) -> int:
    """Suggested cap = max * margin, rounded UP to a multiple of 16 (warp-friendly)."""
    target = math.ceil(max_count * margin)
    return int(math.ceil(target / 16) * 16)


def histogram(counts: np.ndarray, bins: int = 10) -> str:
    hi = max(int(counts.max()), 1)
    edges = np.linspace(0, hi, bins + 1)
    hist, _ = np.histogram(counts, bins=edges)
    peak = max(int(hist.max()), 1)
    lines = []
    for i in range(bins):
        bar = "#" * int(40 * hist[i] / peak)
        lines.append(f"    [{edges[i]:5.0f},{edges[i+1]:5.0f}) {hist[i]:8d} {bar}")
    return "\n".join(lines)


def report_asset(asset: Asset, args, rng: np.random.Generator) -> dict:
    rt = quat_to_world_to_local(asset.quat)
    pts = sample_points(asset, args.point_samples, rng)
    rays_o, rays_d = sample_rays(asset, args.ray_samples, rng)

    pt_counts = point_overlap_counts(asset, pts, rt, chunk=args.chunk)
    ray_counts = ray_cross_counts(asset, rays_o, rays_d, rt, chunk=args.chunk)

    ps, rs = stats(pt_counts), stats(ray_counts)
    cap_active = suggest_cap(ps["max"], args.margin)
    cap_hit = suggest_cap(rs["max"], args.margin)

    print(f"\n=== {asset.name}  ({asset.count} primitives, 3-sigma criterion) ===")
    print(f"  point overlap (-> MAX_ACTIVE_PRIMS):    "
          f"max={ps['max']:4d}  p99={ps['p99']:4d}  p50={ps['p50']:4d}  mean={ps['mean']:.1f}")
    print(histogram(pt_counts))
    print(f"  ray entries   (-> HIT_BUFFER_CAPACITY): "
          f"max={rs['max']:4d}  p99={rs['p99']:4d}  p50={rs['p50']:4d}  mean={rs['mean']:.1f}")
    print(histogram(ray_counts))
    print(f"  suggested caps (margin {args.margin:.2f}, rounded to 16): "
          f"MAX_ACTIVE_PRIMS >= {cap_active}, HIT_BUFFER_CAPACITY >= {cap_hit}")

    for label, mx, cur in (("MAX_ACTIVE_PRIMS", ps["max"], CURRENT_MAX_ACTIVE_PRIMS),
                           ("HIT_BUFFER_CAPACITY", rs["max"], CURRENT_HIT_BUFFER_CAPACITY)):
        if mx > cur:
            print(f"  !! WARNING: observed {label} max {mx} EXCEEDS current cap {cur} "
                  f"-- this asset will overflow (under-absorption); recompile with a larger cap.")

    return {"asset": asset.name, "count": asset.count,
            "active_max": ps["max"], "active_p99": ps["p99"],
            "hit_max": rs["max"], "hit_p99": rs["p99"],
            "cap_active": cap_active, "cap_hit": cap_hit}


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("ply", nargs="+", type=Path, help="PLY asset file(s)")
    p.add_argument("--margin", type=float, default=1.25,
                   help="safety multiplier on the observed max (default 1.25)")
    p.add_argument("--point-samples", type=int, default=50_000,
                   help="random AABB query points (centers are always included)")
    p.add_argument("--ray-samples", type=int, default=200_000,
                   help="random lines through the AABB")
    p.add_argument("--chunk", type=int, default=256,
                   help="query/ray chunk size (lower if memory-bound)")
    p.add_argument("--seed", type=int, default=0, help="RNG seed (reproducible)")
    p.add_argument("--csv", type=Path, help="also write the per-asset summary to CSV")
    args = p.parse_args()

    rng = np.random.default_rng(args.seed)
    rows = []
    for path in args.ply:
        try:
            asset = load_ply(path)
        except (OSError, ValueError) as e:
            print(f"ERROR loading {path}: {e}", file=sys.stderr)
            continue
        rows.append(report_asset(asset, args, rng))

    if args.csv and rows:
        import csv
        with open(args.csv, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
        print(f"\nWrote {args.csv}")

    return 0 if rows else 1


if __name__ == "__main__":
    raise SystemExit(main())
