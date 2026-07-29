import os,numpy as np,OpenEXR,Imath
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
D="renders/cloud_scatter_study"; SPPS=[64,128,256,512,1024]
def load(p):
    f=OpenEXR.InputFile(p);dw=f.header()["dataWindow"]
    sz=(dw.max.y-dw.min.y+1,dw.max.x-dw.min.x+1)
    ch=f.channels(["R","G","B"],Imath.PixelType(Imath.PixelType.FLOAT))
    return np.stack([np.frombuffer(c,np.float32).reshape(sz) for c in ch],-1).mean(-1)
cuda={s:load(f"{D}/cuda_spp{s}.exr") for s in SPPS}
mits={s:load(f"{D}/mitsuba_spp{s}.exr") for s in SPPS}
# orientation: align mitsuba to cuda using highest spp
ref=SPPS[-1]
if np.mean((cuda[ref]-mits[ref])**2)>np.mean((cuda[ref]-np.flipud(mits[ref]))**2):
    mits={s:np.flipud(v) for s,v in mits.items()}; print("[flipped mitsuba]")
def met(a,b):
    d=a-b;mse=float((d**2).mean())
    return mse**0.5,(10*np.log10(1/mse) if mse>0 else 99),abs(float(d.mean())),float(d.std())
# timing
tim={}
if os.path.exists(f"{D}/timing.txt"):
    for ln in open(f"{D}/timing.txt"):
        r,s,t=ln.split(); tim[(r,int(s))]=int(t)
print("\n=== CROSS: CUDA(spp) vs Mitsuba(spp), same spp ===")
print(f"{'spp':>5} {'RMSE':>8} {'PSNR':>7} {'systematic':>11} {'noise':>8} {'tCUDA':>7} {'tMits':>7}")
for s in SPPS:
    r,p,sy,no=met(cuda[s],mits[s])
    print(f"{s:>5} {r:8.5f} {p:7.2f} {sy:11.5f} {no:8.5f} {tim.get(('cuda',s),'?'):>6}s {tim.get(('mitsuba',s),'?'):>6}s")
print("\n=== SELF-CONVERGENCE vs own best (spp=1024) — pure MC noise ===")
print(f"{'spp':>5} {'CUDA RMSE':>10} {'Mits RMSE':>10}")
for s in SPPS[:-1]:
    rc,_,_,_=met(cuda[s],cuda[ref]); rm,_,_,_=met(mits[s],mits[ref])
    print(f"{s:>5} {rc:10.5f} {rm:10.5f}")
# best-vs-best systematic, with regional core check
a,b=cuda[ref],mits[ref]; d=a-b
H,W=a.shape; cy,cx=H//2,W//2
print(f"\n=== SYSTEMATIC at spp={ref} (best estimate of true disagreement) ===")
print(f"global: mean diff {d.mean():+.6f}  RMSE {np.sqrt((d**2).mean()):.5f}")
# mask the cloud body (where mitsuba clearly < background ~1) to test the dense core
body=b<0.6
print(f"dense body (ref<0.6, n={body.sum()}): mean diff {d[body].mean():+.6f}")
for r in [16,48,96]:
    sa=a[cy-r:cy+r,cx-r:cx+r];sb=b[cy-r:cy+r,cx-r:cx+r]
    print(f"central {2*r}x{2*r}: CUDA {sa.mean():.5f} Mits {sb.mean():.5f} diff {sa.mean()-sb.mean():+.6f}")
# plots
fig,ax=plt.subplots(1,2,figsize=(13,5))
cr=[met(cuda[s],mits[s])[0] for s in SPPS]
cs=[met(cuda[s],cuda[ref])[0] for s in SPPS[:-1]]; ms=[met(mits[s],mits[ref])[0] for s in SPPS[:-1]]
ax[0].loglog(SPPS,cr,'o-',label="CUDA vs Mitsuba (cross)")
ax[0].loglog(SPPS[:-1],cs,'s--',label="CUDA self-conv")
ax[0].loglog(SPPS[:-1],ms,'^--',label="Mitsuba self-conv")
ax[0].loglog(SPPS,[cr[0]*(SPPS[0]/s)**0.5 for s in SPPS],'k:',alpha=0.5,label="ideal 1/sqrt(spp)")
ax[0].set_xlabel("spp");ax[0].set_ylabel("RMSE");ax[0].legend(fontsize=8);ax[0].grid(True,which='both',alpha=0.3)
ax[0].set_title("Convergence (cross-RMSE is noise if it tracks 1/sqrt(spp))")
mm=np.abs(d).max()
im=ax[1].imshow(d,cmap="RdBu_r",vmin=-mm,vmax=mm);ax[1].axis("off")
ax[1].set_title(f"diff at spp={ref}  mean={d.mean():+.5f}")
fig.colorbar(im,ax=ax[1],fraction=0.046)
fig.tight_layout(); fig.savefig(f"{D}/cloud_convergence.png",dpi=120); print(f"\nsaved {D}/cloud_convergence.png")
# final 3-panel
fig,ax=plt.subplots(1,3,figsize=(14,4.8))
ax[0].imshow(np.clip(a,0,1),cmap="gray",vmin=0,vmax=1);ax[0].set_title(f"CUDA spp={ref}");ax[0].axis("off")
ax[1].imshow(np.clip(b,0,1),cmap="gray",vmin=0,vmax=1);ax[1].set_title(f"Mitsuba spp={ref}");ax[1].axis("off")
im=ax[2].imshow(d,cmap="RdBu_r",vmin=-mm,vmax=mm);ax[2].set_title(f"diff RMSE={np.sqrt((d**2).mean()):.4f}");ax[2].axis("off")
fig.colorbar(im,ax=ax[2],fraction=0.046);fig.tight_layout()
fig.savefig(f"{D}/cloud_compare_best.png",dpi=120); print(f"saved {D}/cloud_compare_best.png")
