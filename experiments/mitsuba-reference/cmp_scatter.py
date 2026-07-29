import numpy as np, OpenEXR, Imath, sys
def load(p):
    f=OpenEXR.InputFile(p); dw=f.header()["dataWindow"]
    sz=(dw.max.y-dw.min.y+1,dw.max.x-dw.min.x+1)
    ch=f.channels(["R","G","B"],Imath.PixelType(Imath.PixelType.FLOAT))
    return np.stack([np.frombuffer(c,np.float32).reshape(sz) for c in ch],-1).mean(-1)
a=load(sys.argv[1]); b=load(sys.argv[2])
if a.shape==b.shape and np.mean((a-b)**2)>np.mean((a-np.flipud(b))**2): b=np.flipud(b); print("[flipped ref]")
d=a-b
print(f"CUDA    mean={a.mean():.5f} min={a.min():.5f} max={a.max():.5f}")
print(f"Mitsuba mean={b.mean():.5f} min={b.min():.5f} max={b.max():.5f}")
print(f"diff    mean={d.mean():+.5f}  RMSE={np.sqrt((d**2).mean()):.5f}  maxabs={np.abs(d).max():.5f}")
print("\n by ref-radiance bin:  n   meanCUDA  meanMits   meanDiff")
edges=np.linspace(b.min(),b.max(),9)
for i in range(8):
    m=(b>=edges[i])&(b<edges[i+1])
    if m.sum()==0: continue
    print(f"  [{edges[i]:.3f},{edges[i+1]:.3f})  {m.sum():6d}  {a[m].mean():.5f}  {b[m].mean():.5f}  {d[m].mean():+.5f}")
