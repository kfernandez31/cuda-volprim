import numpy as np, mitsuba as mi, math
mi.set_variant('cuda_ad_rgb')

ORTHO=6.0; W=H=256
img = np.array(mi.Bitmap('/tmp/sg_overlap/single_gaussian_validation/0000.exr')).astype(np.float32).mean(-1)

# 4 coincident M=1 Gaussians => total mass 4. tau(d)=4/(2pi)*exp(-d^2/2)
M=4.0
js=(np.arange(W)+0.5)/W*ORTHO-ORTHO/2
PX,PY=np.meshgrid(js,js)
tau=(M/(2*math.pi))*np.exp(-0.5*(PX*PX+PY*PY))
H=np.exp(-tau)

def g(a): return (np.clip(a,0,1)**(1/2.2)*255).astype(np.uint8)
panel=np.ones((H.shape[0], W*3+20), np.float32)
panel[:,:W]=H                      # analytic exp(-4 tau)
panel[:,W+10:2*W+10]=img           # CUDA 4 coincident
panel[:,2*W+20:]=np.clip(np.abs(img-H)*8,0,1)  # 8x diff
out='/home/kacper/thesis/test_results/OVERLAP_4coincident_analytic_vs_cuda.png'
mi.Bitmap(g(panel[...,None].repeat(3,-1)),pixel_format=mi.Bitmap.PixelFormat.RGB).write(out)
print("wrote",out)
print("layout: analytic exp(-4tau) | CUDA 4-coincident | 8x|diff|")
print("RMSE=%.5f"%float(np.sqrt(((img-H)**2).mean())))
