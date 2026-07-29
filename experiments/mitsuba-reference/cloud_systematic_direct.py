# Direct (non-extrapolated) measurement of the CUDA-vs-Mitsuba systematic on the cloud,
# from multi-seed averages CUDA* and M*. Seed differences within one renderer are
# systematic-free (same bias cancels) -> clean per-renderer noise -> exact error bars.
import glob,numpy as np,OpenEXR,Imath
from scipy.ndimage import gaussian_filter
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
CD="renders/cloud_converge"; SD="renders/cloud_scatter_study"
def load(p):
    f=OpenEXR.InputFile(p);dw=f.header()["dataWindow"]
    sz=(dw.max.y-dw.min.y+1,dw.max.x-dw.min.x+1)
    ch=f.channels(["R","G","B"],Imath.PixelType(Imath.PixelType.FLOAT))
    return np.stack([np.frombuffer(c,np.float32).reshape(sz) for c in ch],-1).mean(-1)
cuda_files=sorted(glob.glob(f"{CD}/cuda_s512_seed*.exr"))
mits_files=sorted(glob.glob(f"{CD}/mitsuba_s512_seed*.exr"))
C=[load(p) for p in cuda_files]; M=[load(p) for p in mits_files]
nC,nM=len(C),len(M)
Cs=np.mean(C,0); Ms=np.mean(M,0)
if np.mean((Cs-Ms)**2)>np.mean((Cs-np.flipud(Ms))**2):
    Ms=np.flipud(Ms); M=[np.flipud(x) for x in M]
print(f"CUDA* = {nC} seeds @512  |  M* = {nM} seeds @512")
def seed_noise(stack):  # single-seed noise from systematic-free pair diffs
    ds=[np.std(stack[i]-stack[j])/np.sqrt(2) for i in range(len(stack)) for j in range(i+1,len(stack))]
    return float(np.mean(ds))
sC=seed_noise(C)/np.sqrt(nC); sM=seed_noise(M)/np.sqrt(nM)  # noise of the AVERAGES
comb=np.sqrt(sC**2+sM**2)
print(f"residual noise: CUDA*~{sC:.5f}  M*~{sM:.5f}  combined(diff)~{comb:.5f}")
d=Cs-Ms; H,W=d.shape; cy,cx=H//2,W//2
def sem(K): return comb/np.sqrt(K)
print(f"\n=== DIRECT systematic (CUDA* - M*), measured not extrapolated ===")
g=d.mean(); print(f"global: {g:+.6f}  +/- {sem(H*W):.6f} (SEM)   [{abs(g)/sem(H*W):.1f} sigma]")
for r in [16,48,96,192]:
    reg=d[cy-r:cy+r,cx-r:cx+r]; m=reg.mean(); s=sem((2*r)**2)
    print(f"central {2*r:3d}^2: {m:+.6f} +/- {s:.6f}  [{abs(m)/s:.1f} sigma]")
body=Ms<0.6; mb=d[body].mean(); sb=sem(body.sum())
print(f"dense body(M*<0.6,n={body.sum()}): {mb:+.6f} +/- {sb:.6f}  [{abs(mb)/sb:.1f} sigma]")
db=gaussian_filter(d,sigma=16)
print(f"blurred(sig16): max|.|={np.abs(db).max():.5f} mean={db.mean():+.6f}")
# plot
fig,ax=plt.subplots(1,2,figsize=(13,5))
ax[0].imshow(np.clip(Cs,0,1),cmap="gray",vmin=0,vmax=1);ax[0].set_title(f"CUDA* ({nC} seeds)");ax[0].axis("off")
mm=np.abs(db).max() or 1e-6
im=ax[1].imshow(db,cmap="RdBu_r",vmin=-mm,vmax=mm)
ax[1].set_title(f"DIRECT systematic CUDA*-M* (blurred s16)\nglobal {g:+.5f}+/-{sem(H*W):.5f}");ax[1].axis("off")
fig.colorbar(im,ax=ax[1],fraction=0.046);fig.tight_layout()
fig.savefig(f"{CD}/cloud_systematic_direct.png",dpi=120);print(f"saved {CD}/cloud_systematic_direct.png")
