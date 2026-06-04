"""Convert a DSYG/GaborVolumes Gaussian-pyramid asset (pyr0, level-0 Gaussian subband
'f0_o-1') from its .npy arrays into our renderer's PLY format.

Asset npy fields (per render_asset_new.py): centers (n,3), scales (n,3, LINEAR),
quaternions (n,4, Mitsuba order x,y,z,w), opacities (n,1 -> sigma_t). Albedo is NOT in
the asset (defaults to grey at render time), so we write albedo=0 (overridden per-render).

Our PLY (src/.../io/ply.cpp) REQUIRES: x,y,z ; rot_0..3 (UnitQuaternion::from(w,x,y,z)) ;
scale_0..2 (LOG-scale — loader applies expf) ; albedo_0..2 ; sigma_t_0 (linear total mass).

Quaternion order is the one convention to verify empirically (env QUAT_ORDER):
  xyzw  -> rot=(w,x,y,z)=(q3,q0,q1,q2)   [default; Mitsuba (x,y,z,w) -> our (w,x,y,z)]
  wxyz  -> rot=(q0,q1,q2,q3)
Validate by matching the CUDA render vs render_asset_new.py (Mitsuba). If mirrored/rotated
wrong, try the other order.

Usage: npy_asset_to_ply.py <asset_pyr0_dir> <out.ply>   [env QUAT_ORDER=xyzw|wxyz]
"""
import sys, os, glob, struct
import numpy as np

asset_dir = sys.argv[1]
out_ply = sys.argv[2]
QUAT_ORDER = os.environ.get("QUAT_ORDER", "xyzw")

def find(field):
    # level-0 Gaussian subband is 'f0_o-1'; fall back to any matching field file
    for pat in (f"{field}_f0_o-1.npy", f"{field}_f0_o*.npy", f"{field}_*.npy", f"{field}.npy"):
        hits = sorted(glob.glob(os.path.join(asset_dir, "**", pat), recursive=True))
        if hits:
            return hits[0]
    raise FileNotFoundError(f"no {field}_*.npy under {asset_dir}")

centers = np.load(find("centers")).astype(np.float64).reshape(-1, 3)
scales  = np.load(find("scales")).astype(np.float64).reshape(-1, 3)
quats   = np.load(find("quaternions")).astype(np.float64).reshape(-1, 4)
opac    = np.load(find("opacities")).astype(np.float64).reshape(-1)
n = len(centers)
assert len(scales) == n and len(quats) == n and len(opac) == n, "field length mismatch"
print(f"{n} primitives | center range {centers.min():.3f}..{centers.max():.3f} | "
      f"scale {scales.min():.4f}..{scales.max():.4f} | opacity {opac.min():.4f}..{opac.max():.4f}")

# quaternion -> our (w,x,y,z)
if QUAT_ORDER == "xyzw":
    w, x, y, z = quats[:, 3], quats[:, 0], quats[:, 1], quats[:, 2]
elif QUAT_ORDER == "wxyz":
    w, x, y, z = quats[:, 0], quats[:, 1], quats[:, 2], quats[:, 3]
else:
    raise ValueError("QUAT_ORDER must be xyzw or wxyz")

log_scale = np.log(np.clip(scales, 1e-12, None))  # our loader applies expf

props = ["x","y","z","rot_0","rot_1","rot_2","rot_3",
         "scale_0","scale_1","scale_2","albedo_0","albedo_1","albedo_2","sigma_t_0"]
rows = np.zeros((n, len(props)), dtype=np.float32)
rows[:,0:3]   = centers
rows[:,3]=w; rows[:,4]=x; rows[:,5]=y; rows[:,6]=z
rows[:,7:10]  = log_scale
rows[:,10:13] = 0.0                 # albedo (overridden at render)
rows[:,13]    = opac                # sigma_t = total mass (scaler applied via --sigma-multiplier)

with open(out_ply, "wb") as f:
    hdr = "ply\nformat binary_little_endian 1.0\n"
    hdr += f"element vertex {n}\n"
    hdr += "".join(f"property float {p}\n" for p in props)
    hdr += "end_header\n"
    f.write(hdr.encode("ascii"))
    f.write(rows.tobytes())
print(f"wrote {out_ply}  ({n} prims, QUAT_ORDER={QUAT_ORDER})")
