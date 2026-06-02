"""
Independent double-precision brute-force transmittance for the cloud (cam_0),
absorption (albedo=0). Neutral ground truth: NO BVH, NO Monte Carlo, NO renderer
code — analytic 3σ-truncated-chord optical depth summed over all 652 ellipsoids
in float64.

SS (env, default 1): supersample factor per axis. SS=1 -> point-sample at pixel
center. SS=4 -> 4x4=16 uniform sub-pixel samples averaged = BOX-filter antialias
(matches CUDA's box AA). Output saved to /tmp/cloud_spp/bruteforce_T[_ssN].npy.
"""
import os, sys
import numpy as np
from os.path import join
sys.path.insert(0, '/home/kacper/volumetric_primitives')
import mitsuba as mi
mi.set_variant('cuda_ad_rgb')
import volprim.integrators.volprim_prb  # noqa
from scipy.special import erf
CLOUD='/home/kacper/thesis/assets/cloud'; sys.path.insert(0, CLOUD)
import __init__ as cs

SIG=7.5
SS=int(os.environ.get("SS","1"))
d = {'type':'scene'}; d.update(cs.OBJECTS); d.update(cs.EMITTERS)
d.pop('resources',None); d['primitives_pyr0'].pop('extent_adaptive_clamping',None)
d['primitives_pyr0']['filename']=join(CLOUD,'data/root.primitives_pyr0.ply')
cam=sorted(k for k in cs.SENSORS if k.startswith('cam_'))[0]
cc=cs.SENSORS[cam].copy(); cc.pop('resources',None); d[cam]=cc
scene=mi.load_dict(d); p=mi.traverse(scene)
data=np.array(p['primitives_pyr0.data']).reshape(-1,10).astype(np.float64)
centers=data[:,0:3]; scales=data[:,3:6]; quats=data[:,6:10]
sigma=np.array(p['primitives_pyr0.sigma_t']).reshape(-1).astype(np.float64)*SIG
ext=np.array(p['primitives_pyr0.extent']).reshape(-1).astype(np.float64)
EXT2=(ext*ext) if ext.size>1 else float(ext[0])**2
N=len(centers); print(f"N={N} SS={SS}")
sensor=scene.sensors()[0]; W,H=sensor.film().size()

def quat_R(q):
    x,y,z,w=q
    return np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],
                     [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],
                     [2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]],dtype=np.float64)
RsT=[quat_R(quats[i]).T for i in range(N)]
prodinv=1.0/(scales[:,0]*scales[:,1]*scales[:,2])
INV2PI=1.0/(2*np.pi); R2=np.sqrt(2.0)
e2arr = EXT2 if hasattr(EXT2,'__len__') else np.full(N,EXT2)

def tau_for(O,D):
    tau=np.zeros(O.shape[0])
    for i in range(N):
        s=scales[i]; c=centers[i]; RT=RsT[i]
        p_=((O-c)@RT.T)/s; w_=(D@RT.T)/s
        a=np.einsum('ij,ij->i',w_,w_); pw=np.einsum('ij,ij->i',p_,w_); pp=np.einsum('ij,ij->i',p_,p_)
        b=2*pw; disc=b*b-4*a*(pp-e2arr[i]); m=disc>0
        if not m.any(): continue
        sq=np.sqrt(np.where(m,disc,0.0)); ta=(-b-sq)/(2*a); tb=(-b+sq)/(2*a)
        t0=np.maximum(ta,0.0); t1=tb; m=m&(t1>t0)
        tc=-pw/a; perp2=pp-pw*pw/a; wl=np.sqrt(a)
        contrib=sigma[i]*INV2PI*prodinv[i]*np.exp(-0.5*perp2)*(1.0/wl)*0.5*(erf((t1-tc)*wl/R2)-erf((t0-tc)*wl/R2))
        tau+=np.where(m,np.maximum(contrib,0.0),0.0)
    return tau

offs=(np.arange(SS)+0.5)/SS
Tacc=np.zeros(H*W)
for sj in offs:
    for si in offs:
        xs=(np.arange(W)+si)/W; ys=(np.arange(H)+sj)/H
        gx,gy=np.meshgrid(xs,ys)
        ray,_=sensor.sample_ray(0.0,0.0,mi.Point2f(gx.ravel(),gy.ravel()),mi.Point2f(0.5,0.5))
        oo=np.array(ray.o); dd=np.array(ray.d)
        O=(oo.T if oo.shape[0]==3 else oo).astype(np.float64).reshape(-1,3)
        D=(dd.T if dd.shape[0]==3 else dd).astype(np.float64).reshape(-1,3)
        Tacc+=np.exp(-tau_for(O,D))
    print(f"  subrow {sj:.3f} done")
T=(Tacc/(SS*SS)).reshape(H,W).astype(np.float32)

import OpenEXR, Imath
def load(pth):
    f=OpenEXR.InputFile(pth); dw=f.header()["dataWindow"]
    sz=(dw.max.y-dw.min.y+1,dw.max.x-dw.min.x+1)
    ch=f.channels(["R","G","B"],Imath.PixelType(Imath.PixelType.FLOAT))
    return np.stack([np.frombuffer(cx,np.float32).reshape(sz) for cx in ch],-1).mean(-1)
cuda=load("/home/kacper/thesis/renders/cloud_lifted/cloud_asset_validation/0000.exr")
if np.mean((T-cuda)**2) > np.mean((np.flipud(T)-cuda)**2): T=np.flipud(T); print("flipped V")
out=f"/tmp/cloud_spp/bruteforce_T{'' if SS==1 else f'_ss{SS}'}.npy"
np.save(out,T); print(f"saved {out} mean={T.mean():.4f}")
