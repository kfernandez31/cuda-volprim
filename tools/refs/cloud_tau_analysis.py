import numpy as np, mitsuba as mi
mi.set_variant('cuda_ad_rgb')
def load(p): return np.array(mi.Bitmap(p)).astype(np.float32).mean(-1)
c = load('/tmp/cloud_verify/cloud_asset_validation/0000.exr')
m = load('/home/kacper/thesis/test_results/ply_via_mitsuba/cam_0000_volprim_tomography_spp1024.exr')
def tau(T): return -np.log(np.clip(T, 1e-4, 1.0))
tc, tm = tau(c), tau(m)
mid = (m > 0.1) & (m < 0.9)
ratio = tc[mid] / np.clip(tm[mid], 1e-3, None)
print("mid-range px:", int(mid.sum()))
print("tau ratio CUDA/Mits  median=%.3f  mean=%.3f  p25=%.3f p75=%.3f"
      % (np.median(ratio), ratio.mean(), np.percentile(ratio,25), np.percentile(ratio,75)))
# finite-tau global (exclude saturated)
fin = (c > 1e-3) & (m > 1e-3)
print("finite-both px tau: CUDA=%.3f Mits=%.3f ratio=%.3f"
      % (tc[fin].mean(), tm[fin].mean(), tc[fin].mean()/max(tm[fin].mean(),1e-9)))
