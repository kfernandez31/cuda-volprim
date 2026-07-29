"""General multi-seed systematic: diff-of-averages between two seed sets.

Seed differences WITHIN one renderer are systematic-free (same bias cancels) → clean
per-renderer noise → honest error bars even with heavy-tailed fireflies. The diff of the
two averages is the systematic (bias); noise → 0 as #seeds·spp grows. SEM is derived from
seed-pair stds, so it correctly inflates under fireflies.

Usage:
    sg_systematic.py "renders/sg_meadow/cuda_seed*.exr" "renders/sg_meadow/mits_seed*.exr"
"""
import sys, glob
import numpy as np
import OpenEXR, Imath


def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()["dataWindow"]
    sz = (dw.max.y - dw.min.y + 1, dw.max.x - dw.min.x + 1)
    ch = f.channels(["R", "G", "B"], Imath.PixelType(Imath.PixelType.FLOAT))
    return np.stack([np.frombuffer(c, np.float32).reshape(sz) for c in ch], -1)


C = [load(p) for p in sorted(glob.glob(sys.argv[1]))]
M = [load(p) for p in sorted(glob.glob(sys.argv[2]))]
assert C and M, "no files matched"
nC, nM = len(C), len(M)
Cs, Ms = np.mean(C, 0), np.mean(M, 0)
# Auto vertical-flip Mitsuba if it reduces MSE (save convention may differ).
if np.mean((Cs - Ms) ** 2) > np.mean((Cs - np.flipud(Ms)) ** 2):
    Ms = np.flipud(Ms); M = [np.flipud(x) for x in M]; print("[flipped Mitsuba]")
print(f"CUDA* = {nC} seeds  |  M* = {nM} seeds   shape={Cs.shape}")


def seed_noise(stack):  # single-seed per-pixel noise from systematic-free pair diffs
    ds = [np.std(stack[i] - stack[j]) / np.sqrt(2)
          for i in range(len(stack)) for j in range(i + 1, len(stack))]
    return float(np.mean(ds))


sC = seed_noise(C) / np.sqrt(nC)
sM = seed_noise(M) / np.sqrt(nM)
comb = np.sqrt(sC ** 2 + sM ** 2)   # per-pixel noise of the difference image
print(f"residual noise (avg): CUDA*~{sC:.5f}  M*~{sM:.5f}  combined~{comb:.5f}")

d = Cs - Ms
H, W, _ = d.shape
npix = H * W * 3


def sem(npx):  # SEM of a regional mean over npx samples
    return comb / np.sqrt(npx)


print(f"CUDA*    mean={Cs.mean():.5f}  Mits*    mean={Ms.mean():.5f}")
print("\n=== DIRECT systematic (CUDA* - M*) ===")
g = d.mean()
s = sem(npix)
print(f"global: {g:+.6f}  +/- {s:.6f} (SEM)   [{abs(g)/s:.1f} sigma]")
for nm, idx in [("R", 0), ("G", 1), ("B", 2)]:
    dc = d[..., idx]; sc = comb / np.sqrt(H * W)
    print(f"  {nm}: {dc.mean():+.6f} +/- {sc:.6f}  [{abs(dc.mean())/sc:.1f} sigma]")
# robust: median diff and clipped-mean diff (firefly-insensitive cross-checks)
print(f"\nmedian|diff|={np.median(np.abs(d)):.6f}   "
      f"clipped(<5) global diff={np.clip(Cs,0,5).mean()-np.clip(Ms,0,5).mean():+.6f}   "
      f"clipped(<2) global diff={np.clip(Cs,0,2).mean()-np.clip(Ms,0,2).mean():+.6f}")
