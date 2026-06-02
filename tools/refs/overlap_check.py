import numpy as np, mitsuba as mi, math
mi.set_variant('cuda_ad_rgb')

ORTHO=6.0; W=H=256
img = np.array(mi.Bitmap('/tmp/sg_overlap/single_gaussian_validation/0000.exr')).astype(np.float32).mean(-1)

# 4 coincident M=1 Gaussians => total mass M=4. Closed form tau(d)=M/(2pi)*exp(-d^2/2).
M = 4.0
js = (np.arange(W)+0.5)/W*ORTHO - ORTHO/2
PX,PY = np.meshgrid(js, js)
d2 = PX*PX + PY*PY
tau = (M/(2*math.pi))*np.exp(-0.5*d2)
H = np.exp(-tau)

rmse = float(np.sqrt(((img-H)**2).mean()))
mid = (H>0.2)&(H<0.8)
rmse_mid = float(np.sqrt(((img[mid]-H[mid])**2).mean()))
print("4 coincident M=1 Gaussians vs analytic exp(-4*tau):")
print("  render mean=%.4f  analytic mean=%.4f"%(img.mean(), H.mean()))
print("  RMSE=%.5f   mid-range(0.2-0.8) RMSE=%.5f  (%d px)"%(rmse, rmse_mid, int(mid.sum())))
print("  center px: render=%.4f analytic=%.4f"%(img[128,128], H[128,128]))
