#!/usr/bin/env python3
"""Three-way converged image means +- SE (thesis Sec 5.5 unbiasedness paragraph).

Arms (meadow illumination, showcase cloud):
  ours          = 18-render spp-weighted pool (the fig:g1-bias recipe), 40960 spp effective
  ref analog    = g1_seeds/mits_seed*.exr (16 x 64 spp, Mitsuba analog)
  ref corr. NEE = nee_fair/gt_fix5/gabor_nee_meadow_spp2048_seed*.exr (4 x 2048 spp)

SEs are seed-spread standard errors of the IMAGE MEAN. The ours pool mixes spp classes,
so its SE uses the variance model Var_i = c/spp_i with c estimated from the 8x1024 class.
Run under experiments/mitsuba-reference/.venv (needs mitsuba). Output (2026-07-16):
  ours 0.32149 +- 0.00001 | analog 0.32009 +- 0.00397 | corrNEE 0.32310 +- 0.00003
  ours-vs-analog 0.35 SE apart; extreme spread of the triple 0.94%.
"""
import glob
import numpy as np
import mitsuba as mi

mi.set_variant("scalar_rgb")


def image_mean(path):
    return float(np.array(mi.Bitmap(path)).astype(np.float64)[..., :3].mean())


CC = "results/campaign/cloud_conv"
ours_files = (
    [(f"{CC}/ours_spp1024_seed{s}.exr", 1024) for s in range(1, 9)]
    + [(f"{CC}/ours_spp2048_seed{s}.exr", 2048) for s in (5, 6, 7, 8)]
    + [(f"{CC}/ours_spp4096_seed{s}.exr", 4096) for s in (5, 6, 7, 8)]
    + [(f"{CC}/ours_gt4096_seed{s}.exr", 4096) for s in (1, 2)]
)
mo = np.array([image_mean(f) for f, _ in ours_files])
spp = np.array([s for _, s in ours_files], float)
w = spp / spp.sum()
mu_ours = float((mo * w).sum())
c = np.var(mo[:8], ddof=1) * 1024  # per-seed image-mean variance model, from the 1024 class
se_ours = float(np.sqrt((w**2 * (c / spp)).sum()))

ma = np.array([image_mean(f) for f in sorted(glob.glob("results/campaign/g1_seeds/mits_seed*.exr"))])
mn = np.array([image_mean(f) for f in sorted(
    glob.glob("results/campaign/nee_fair/gt_fix5/gabor_nee_meadow_spp2048_seed*.exr"))])

mu_a, se_a = ma.mean(), ma.std(ddof=1) / np.sqrt(len(ma))
mu_n, se_n = mn.mean(), mn.std(ddof=1) / np.sqrt(len(mn))
common = (mu_ours + mu_a + mu_n) / 3

print(f"ours     {mu_ours:.5f} +- {se_ours:.5f}  (n={len(mo)} weighted pool)")
print(f"analog   {mu_a:.5f} +- {se_a:.5f}  (n={len(ma)} x 64 spp)")
print(f"corrNEE  {mu_n:.5f} +- {se_n:.5f}  (n={len(mn)} x 2048 spp)")
print(f"ours vs analog: {abs(mu_ours - mu_a) / se_a:.2f} SE apart")
print(f"extreme spread of the triple: {100 * (mu_n - mu_a) / common:.2f}%")
