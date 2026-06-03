import os,glob,numpy as np,OpenEXR,Imath
from scipy.ndimage import gaussian_filter
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
SD="renders/cloud_scatter_study"; CD="renders/cloud_converge"
def load(p):
    f=OpenEXR.InputFile(p);dw=f.header()["dataWindow"]
    sz=(dw.max.y-dw.min.y+1,dw.max.x-dw.min.x+1)
    ch=f.channels(["R","G","B"],Imath.PixelType(Imath.PixelType.FLOAT))
    return np.stack([np.frombuffer(c,np.float32).reshape(sz) for c in ch],-1).mean(-1)
SPPS=[64,128,256,512,1024]
cuda={s:load(f"{SD}/cuda_spp{s}.exr") for s in SPPS}
# converged Mitsuba M*: all multi-seed @512 + the study seed0 @512
seeds=sorted(glob.glob(f"{CD}/mitsuba_s512_seed*.exr"))
imgs=[load(p) for p in seeds]
if os.path.exists(f"{SD}/mitsuba_spp512.exr"): imgs.append(load(f"{SD}/mitsuba_spp512.exr"))
N=len(imgs); Mstar=np.mean(imgs,axis=0)
# align orientation to CUDA
if np.mean((cuda[1024]-Mstar)**2)>np.mean((cuda[1024]-np.flipud(Mstar))**2):
    Mstar=np.flipud(Mstar); imgs=[np.flipud(x) for x in imgs]; print("[flipped M*]")
# M* self-noise: split halves
h=N//2; A=np.mean(imgs[:h],0); B=np.mean(imgs[h:2*h],0)
half_noise=np.std(A-B)/np.sqrt(2)          # noise of a half-average (h seeds)
Mstar_noise=half_noise*np.sqrt(h)/np.sqrt(N)  # scale to full N-average
print(f"M* = mean of {N} Mitsuba seeds @512spp (eff ~{512*N}spp). estimated M* noise ~{Mstar_noise:.5f}")
def stats(a,b):
    d=a-b;return np.sqrt((d**2).mean()),abs(float(d.mean()))
print("\n=== CUDA(spp) vs converged M* (clean reference) ===")
print(f"{'spp':>5} {'RMSE':>8} {'systematic(mean)':>16}")
for s in SPPS:
    r,sy=stats(cuda[s],Mstar); print(f"{s:>5} {r:8.5f} {sy:16.5f}")
# Does CUDA-vs-M* track 1/sqrt(spp) with floor = M* noise (NOT a systematic error)?
# model RMSE^2 = kC^2/spp + Mstar_noise^2 + systematic^2 ; fit kC^2 and floor
import numpy as np
x=np.array([1/s for s in SPPS]); y=np.array([stats(cuda[s],Mstar)[0]**2 for s in SPPS])
A_=np.vstack([x,np.ones_like(x)]).T; kC2,floor2=np.linalg.lstsq(A_,y,rcond=None)[0]
floor=np.sqrt(max(floor2,0))
print(f"\nfit RMSE^2 = {kC2:.4f}/spp + {floor2:.6f}")
print(f"  -> CUDA noise const kC={np.sqrt(max(kC2,0)):.4f}; extrapolated floor(spp->inf)={floor:.5f}")
print(f"  M* residual noise ~{Mstar_noise:.5f}. systematic^2 = floor^2 - Mstar_noise^2 = {max(floor2-Mstar_noise**2,0):.7f}")
sysest=np.sqrt(max(floor2-Mstar_noise**2,0))
print(f"  => SYSTEMATIC (true CUDA error, noise removed) ~ {sysest:.5f}")
# systematic via regional means at 1024 (noise averages out)
a,b=cuda[1024],Mstar; d=a-b; H,W=a.shape; cy,cx=H//2,W//2
print(f"\nregional mean diff (CUDA1024 - M*), noise-averaged:")
print(f"  global: {d.mean():+.6f}")
for r in [16,48,96,192]:
    sd=d[cy-r:cy+r,cx-r:cx+r].mean(); print(f"  central {2*r}x{2*r}: {sd:+.6f}")
# blurred diff map to expose low-freq systematic
db=gaussian_filter(d,sigma=16)
print(f"  blurred(sigma=16) diff: max|.|={np.abs(db).max():.5f}  mean={db.mean():+.6f}")
# plots
fig,ax=plt.subplots(1,2,figsize=(13,5))
rC=[stats(cuda[s],Mstar)[0] for s in SPPS]
ax[0].loglog(SPPS,rC,'o-',label="CUDA vs converged M*")
ax[0].loglog(SPPS,[rC[0]*(SPPS[0]/s)**0.5 for s in SPPS],'k:',label="ideal 1/sqrt(spp)")
ax[0].axhline(Mstar_noise,color='r',ls='--',alpha=0.6,label=f"M* noise floor ~{Mstar_noise:.4f}")
ax[0].set_xlabel("CUDA spp");ax[0].set_ylabel("RMSE vs M*");ax[0].legend(fontsize=8);ax[0].grid(True,which='both',alpha=0.3)
ax[0].set_title("CUDA error vs clean reference: tracks 1/sqrt(spp),\nfloor = M* noise not systematic")
mm=np.abs(db).max()
im=ax[1].imshow(db,cmap="RdBu_r",vmin=-mm,vmax=mm);ax[1].axis("off")
ax[1].set_title(f"blurred(sigma=16) diff CUDA1024 - M*\n(noise suppressed; reveals systematic) max={mm:.4f}")
fig.colorbar(im,ax=ax[1],fraction=0.046);fig.tight_layout()
fig.savefig(f"{CD}/cloud_systematic_proof.png",dpi=120);print(f"\nsaved {CD}/cloud_systematic_proof.png")
