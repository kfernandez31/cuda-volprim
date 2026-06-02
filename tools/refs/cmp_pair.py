"""
Compare a CUDA EXR vs a Mitsuba EXR: global stats, transmittance-binned mean
diff (to expose interior/overlap bias), and a 3-panel figure.

Usage: cmp_pair.py CUDA.exr MITS.exr OUT.png "Title"
"""
import sys
import numpy as np, OpenEXR, Imath
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt


def load(path):
    f = OpenEXR.InputFile(path); dw = f.header()["dataWindow"]
    sz = (dw.max.y - dw.min.y + 1, dw.max.x - dw.min.x + 1)
    ch = f.channels(["R", "G", "B"], Imath.PixelType(Imath.PixelType.FLOAT))
    return np.stack([np.frombuffer(c, np.float32).reshape(sz) for c in ch], -1)


cuda_p, mits_p, out_p, title = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
cuda, mits = load(cuda_p), load(mits_p)
print(f"CUDA  mean={cuda.mean():.6f} min={cuda.min():.6f} max={cuda.max():.6f}")
print(f"Mits  mean={mits.mean():.6f} min={mits.min():.6f} max={mits.max():.6f}")
d = cuda - mits
print(f"diff  mean={d.mean():+.3e}  RMSE={np.sqrt((d**2).mean()):.3e}  "
      f"MAE={np.abs(d).mean():.3e}  maxabs={np.abs(d).max():.3e}")

cl, ml = cuda.mean(-1), mits.mean(-1); dl = cl - ml; p = ml
print(f"\n{'p-bin (Mits)':>16}{'npx':>8}{'meandiff':>12}{'RMSE':>11}")
for lo, hi in [(0.0, 0.2), (0.2, 0.35), (0.35, 0.5), (0.5, 0.65),
               (0.65, 0.8), (0.8, 0.95), (0.95, 1.001)]:
    m = (p >= lo) & (p < hi)
    if m.sum() < 20:
        continue
    print(f"  [{lo:.2f},{hi:.2f}){m.sum():>14}{dl[m].mean():>+12.3e}{np.sqrt((dl[m]**2).mean()):>11.3e}")

fig, ax = plt.subplots(1, 3, figsize=(13, 4.3))
ax[0].imshow(cl, cmap="gray", vmin=0, vmax=1); ax[0].set_title(f"CUDA — {title}")
ax[1].imshow(ml, cmap="gray", vmin=0, vmax=1); ax[1].set_title("Mitsuba prb")
vlim = max(0.01, float(np.abs(dl).max()) * 0.6)
im = ax[2].imshow(dl, cmap="RdBu", vmin=-vlim, vmax=vlim)
ax[2].set_title(f"CUDA - Mitsuba (mean {dl.mean():+.1e})")
plt.colorbar(im, ax=ax[2], fraction=0.046)
for a in ax:
    a.axis("off")
plt.tight_layout(); plt.savefig(out_p, dpi=110)
print(f"\nwrote {out_p}")
