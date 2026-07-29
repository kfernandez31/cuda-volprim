"""RMSE + relative error of test EXR vs reference EXR. Usage: exr_rmse.py test.exr ref.exr"""
import sys
import numpy as np
import OpenEXR, Imath


def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()["dataWindow"]
    sz = (dw.max.y - dw.min.y + 1, dw.max.x - dw.min.x + 1)
    ch = f.channels(["R", "G", "B"], Imath.PixelType(Imath.PixelType.FLOAT))
    return np.stack([np.frombuffer(c, np.float32).reshape(sz) for c in ch], -1)


t, r = load(sys.argv[1]), load(sys.argv[2])
d = t - r
rmse = float(np.sqrt(np.mean(d * d)))
# relative RMSE vs reference mean (a scale-free "how noisy" number)
rel = rmse / (float(r.mean()) + 1e-12)
print(f"{sys.argv[1].split('/')[-1]:<24} RMSE {rmse:.5f}  relRMSE {rel:.4f}  "
      f"meanTest {t.mean():.5f}  meanRef {r.mean():.5f}  biasΔmean {(t.mean()-r.mean()):+.5f}")
