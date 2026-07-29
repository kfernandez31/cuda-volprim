#!/usr/bin/env python3
"""Gate G1 diagnosis: is the ours-MIS vs corrected-NEE pixelwise RMSE (17%) noise or a real bias?

The headline gate reported rel_rmse=17.2% (FAIL @2%) but mean_ratio=0.9965 (PASS). This script
decides between H_noise and H_bias rigorously, using each estimator's OWN inter-seed variance:

  H_noise : the 17% is the two finite-spp GTs' Monte Carlo noise. => chi2 ~ 1, and it averages out
            under spatial pooling (tile rel_rmse falls as 1/blocksize).
  H_bias  : a real systematic per-pixel discrepancy. => chi2 >> 1, tile rel_rmse plateaus at the bias.

Noise-aware chi2 decomposition (luminance):
  diff       = ours_mean - nee_mean
  SEM2_ours  = var_over_seeds(ours, ddof=1) / n_ours       (per-pixel variance of the ours MEAN)
  SEM2_nee   = var_over_seeds(nee,  ddof=1) / n_nee
  chi2       = mean(diff^2) / mean(SEM2_ours + SEM2_nee)    (== 1 if the scatter is pure noise)
  rms_bias   = sqrt(max(0, mean(diff^2) - mean(SEM2_ours+SEM2_nee)))   (the systematic part, if any)

Run under the pip-gabor venv:
  experiments/mitsuba-reference/with_pip_gabor.sh experiments/mitsuba-reference/.venv/bin/python scripts/plots/nee_fair_gate_diagnose.py
"""
import glob, os, sys
import numpy as np
import mitsuba as mi
mi.set_variant("scalar_rgb")

OURS_GLOB = "results/campaign/g1_seeds/cuda_seed*.exr"                       # ours-MIS, 16 x 64 spp
NEE_GLOB  = "results/campaign/nee_fair/gt/gabor_nee_meadow_spp2048_seed*.exr"  # corrected-NEE, 2 x 2048 spp
OUTDIR    = "results/campaign/nee_fair"
LUM_W     = np.array([0.2126, 0.7152, 0.0722], np.float32)

def load_stack(pattern):
    files = sorted(glob.glob(pattern))
    if not files:
        sys.exit(f"[diag] no EXRs match {pattern}")
    return np.stack([np.array(mi.Bitmap(f)).astype(np.float64)[..., :3] for f in files]), files

def lum(rgb):
    return rgb @ LUM_W

def main():
    ours_s, ours_f = load_stack(OURS_GLOB)
    nee_s,  nee_f  = load_stack(NEE_GLOB)
    n_ours, n_nee = len(ours_f), len(nee_f)
    print(f"[diag] ours-MIS {n_ours} seeds (64 spp each -> {64*n_ours} spp eff)")
    print(f"[diag] corrected-NEE {n_nee} seeds (2048 spp each -> {2048*n_nee} spp eff)")

    # per-seed luminance
    ours_l = lum(ours_s)   # (n_ours, H, W)
    nee_l  = lum(nee_s)    # (n_nee,  H, W)
    ours_m, nee_m = ours_l.mean(0), nee_l.mean(0)
    H, W = ours_m.shape

    # per-pixel variance of each MEAN (SEM^2)
    sem2_ours = ours_l.var(0, ddof=1) / n_ours
    sem2_nee  = nee_l.var(0, ddof=1)  / n_nee     # crude (2 seeds) per-pixel, but fine pooled over HxW
    sem2_sum  = sem2_ours + sem2_nee

    diff  = ours_m - nee_m
    ref_mean = float(nee_m.mean())

    md2   = float(np.mean(diff**2))
    msem2 = float(np.mean(sem2_sum))
    chi2  = md2 / msem2
    rms_bias = float(np.sqrt(max(0.0, md2 - msem2)))
    rms_noise = float(np.sqrt(msem2))

    print(f"\n=== noise-aware decomposition (luminance, mean_ref={ref_mean:.4f}) ===")
    print(f"  observed RMSE           : {np.sqrt(md2):.5f}   ({100*np.sqrt(md2)/ref_mean:.2f}% of mean)")
    print(f"  predicted noise RMSE    : {rms_noise:.5f}   ({100*rms_noise/ref_mean:.2f}% of mean)")
    print(f"    - from ours (1024spp) : {np.sqrt(np.mean(sem2_ours)):.5f}   ({100*np.sqrt(np.mean(sem2_ours))/ref_mean:.2f}%)")
    print(f"    - from nee  (4096spp) : {np.sqrt(np.mean(sem2_nee)):.5f}   ({100*np.sqrt(np.mean(sem2_nee))/ref_mean:.2f}%)")
    print(f"  ==> chi2 = obs^2/noise^2: {chi2:.3f}   (==1.0 => pure noise; >>1 => real bias)")
    print(f"  residual RMS bias       : {rms_bias:.5f}   ({100*rms_bias/ref_mean:.2f}% of mean)")

    # Block-downsampling: noise std falls as 1/b, coherent bias plateaus.
    print(f"\n=== block-downsampling: rel_rmse(ours,nee) vs block size (noise must fall ~1/b) ===")
    print(f"  {'block':>6} {'tiles':>10} {'rel_rmse%':>10} {'pred_noise%':>12}")
    for b in (1, 5, 10, 20, 30, 60):
        if H % b or W % b:
            continue
        oa = ours_m.reshape(H//b, b, W//b, b).mean((1, 3))
        na = nee_m.reshape(H//b, b, W//b, b).mean((1, 3))
        s2 = sem2_sum.reshape(H//b, b, W//b, b).mean((1, 3)) / (b*b)  # variance of a b*b-pixel tile mean
        rr = float(np.sqrt(np.mean((oa-na)**2)) / oa.mean())
        pn = float(np.sqrt(np.mean(s2)) / oa.mean())
        print(f"  {b:>6} {f'{H//b}x{W//b}':>10} {100*rr:>10.3f} {100*pn:>12.3f}")

    # Clipped rel_rmse (firefly sensitivity) — mask the top 0.1% |diff| pixels, per the campaign's clip convention.
    absd = np.abs(diff)
    thr = np.percentile(absd, 99.9)
    keep = absd <= thr
    rr_clip = float(np.sqrt(np.mean(diff[keep]**2)) / nee_m[keep].mean())
    print(f"\n  clipped rel_rmse (drop top 0.1% |diff|): {100*rr_clip:.3f}%  (raw was 17.2%)")

    # Spatial-structure dump: 30x30-tile mean diff (coherent structure => bias; grainy => noise).
    b = 30
    tile_diff = (ours_m - nee_m).reshape(H//b, b, W//b, b).mean((1, 3))
    tile_ref  = nee_m.reshape(H//b, b, W//b, b).mean((1, 3))
    tile_rel  = (tile_diff / np.maximum(tile_ref, 1e-3)).astype(np.float32)
    os.makedirs(OUTDIR, exist_ok=True)
    # save signed relative tile diff as grayscale (0.5 = zero) for eyeballing structure
    vis = np.clip(0.5 + 5.0*tile_rel, 0, 1).astype(np.float32)  # +/-10% maps to [0,1]
    mi.Bitmap(np.repeat(np.repeat(np.stack([vis]*3, -1), b, 0), b, 1)).write(
        os.path.join(OUTDIR, "gate_tilediff_signed.exr"))
    print(f"\n  tile (30x30) signed rel diff: min {100*tile_rel.min():.2f}%  max {100*tile_rel.max():.2f}%  "
          f"mean {100*tile_rel.mean():.2f}%  std {100*tile_rel.std():.2f}%")
    print(f"  wrote gate_tilediff_signed.exr (gray=agree, dark=ours<nee, bright=ours>nee, +/-10% full scale)")

if __name__ == "__main__":
    main()
