#!/usr/bin/env python3
"""Task 1 — per-arm convergence (error->0 vs spp), Piotr's ask. Two arms: ours-MIS and corrected-NEE.

Each arm converges to ITS OWN high-spp ground truth (per DECISION.md scope D), which sidesteps the ~5%
peaky-env cross-renderer gap and is the correct way to visualise unbiasedness. GT seeds are DISJOINT
from the per-rung error seeds so the error is not correlated with the GT.

  ours-MIS   GT = mean(cloud_conv ours_spp1024_seed{1,2,3,4})           [4096 spp eff]
             ladder error seeds 5,6,7,8 @ spp {16,64,256,1024}
             (16 from nee_fair/ladder/ours_spp16_seed{5-8}; 64/256/1024 reuse cloud_conv)
  corr-NEE   GT = mean(nee_fair/gt/gabor_nee_meadow_spp2048_seed{0,1})  [4096 spp eff]
             ladder error seeds 2,3,4,5 @ spp {16,64,256,1024}          [nee_fair/ladder/]

Error metric: relative RMSE to GT over luminance, with GT-noise SUBTRACTED (the GT is finite-spp; its
per-pixel variance is estimated from its seed spread and removed so the top rung is not floored by GT
noise). We also print the raw (uncorrected) RMSE and the pure inter-seed std (GT-free) as cross-checks.

Slope ~ -1/2 on log-log with no floor == unbiased + converging. Output: convergence.{pdf,png,csv}.

Run under the pip-gabor venv:
  experiments/mitsuba-reference/with_pip_gabor.sh experiments/mitsuba-reference/.venv/bin/python scripts/plots/nee_fair_convergence.py
"""
import glob, os
import numpy as np
import mitsuba as mi
mi.set_variant("scalar_rgb")
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

LUM = np.array([0.2126, 0.7152, 0.0722], np.float32)
lum = lambda x: x @ LUM
OUT = "results/campaign/nee_fair"
SPPS = [16, 64, 256, 1024, 2048, 4096]

def load(p):
    return np.array(mi.Bitmap(p)).astype(np.float64)[..., :3]

def gt_and_noise(files):
    """Return (GT luminance mean image, per-pixel variance of that MEAN)."""
    ls = np.stack([lum(load(f)) for f in files])          # (n,H,W)
    n = len(files)
    gt = ls.mean(0)
    sem2 = ls.var(0, ddof=1) / n if n > 1 else np.zeros_like(gt)
    return gt, sem2, n

def rung_error(files, gt, gt_sem2):
    """rel RMSE of a single-seed render at this rung vs GT, GT-noise-subtracted, averaged over seeds.
    Returns (corrected_relrmse, raw_relrmse, n_seeds, interseed_std_rel)."""
    ls = np.stack([lum(load(f)) for f in files])          # (n,H,W)
    n = len(files)
    ref = gt.mean()
    # raw: mean over seeds of per-pixel (img-gt)^2
    mse_raw = np.mean([(l - gt) ** 2 for l in ls])
    # subtract GT sampling noise (independent of the rung render, since seeds are disjoint)
    mse_corr = max(0.0, mse_raw - float(gt_sem2.mean()))
    raw = np.sqrt(mse_raw) / ref
    corr = np.sqrt(mse_corr) / ref
    inter = (np.sqrt(ls.var(0, ddof=1).mean()) / ref) if n > 1 else np.nan
    return corr, raw, n, inter

def collect(arm):
    if arm == "ours":
        gt_files = ([f"results/campaign/cloud_conv/ours_spp1024_seed{s}.exr" for s in (1, 2, 3, 4)]
                    + [f"results/campaign/cloud_conv/ours_gt4096_seed{s}.exr" for s in (1, 2)])
        def rung_files(spp):
            if spp == 16:
                return sorted(glob.glob(f"{OUT}/ladder/ours_spp16_seed[5678].exr"))
            return [f"results/campaign/cloud_conv/ours_spp{spp}_seed{s}.exr" for s in (5, 6, 7, 8)]
    else:  # corrected-NEE
        gt_files = [f"{OUT}/gt_fix5/gabor_nee_meadow_spp2048_seed{s}.exr" for s in (0, 1, 6, 7)]
        def rung_files(spp):
            return [f"{OUT}/ladder_fix5/gabor_nee_meadow_spp{spp}_seed{s}.exr" for s in (2, 3, 4, 5)]
    gt_files = [f for f in gt_files if os.path.exists(f)]
    gt, sem2, ngt = gt_and_noise(gt_files)
    rows = []
    for spp in SPPS:
        fs = [f for f in rung_files(spp) if os.path.exists(f)]
        if not fs:
            print(f"  [{arm}] spp{spp}: NO FILES yet, skipping")
            continue
        corr, raw, n, inter = rung_error(fs, gt, sem2)
        rows.append((spp, corr, raw, n, inter))
        print(f"  [{arm}] spp{spp:>4}: n={n} relRMSE corr={100*corr:.2f}% raw={100*raw:.2f}% "
              f"interseed_std={100*inter:.2f}%")
    return rows, ngt, float(gt.mean())

def main():
    print("Collecting convergence data (GT-noise-corrected relative RMSE):")
    data = {}
    for arm in ("ours", "nee"):
        rows, ngt, gtmean = collect(arm)
        data[arm] = rows
        print(f"  [{arm}] GT = {ngt} seeds, GT mean_lum = {gtmean:.4f}")

    # CSV
    os.makedirs(OUT, exist_ok=True)
    with open(f"latex/figures/nee_convergence.csv", "w") as fh:
        fh.write("arm,spp,n_seeds,relrmse_corrected,relrmse_raw,interseed_std_rel\n")
        for arm, rows in data.items():
            for spp, corr, raw, n, inter in rows:
                fh.write(f"{arm},{spp},{n},{corr:.6f},{raw:.6f},{inter:.6f}\n")

    # Plot
    fig, ax = plt.subplots(figsize=(6.2, 4.6))
    styles = {"ours": dict(color="#1f77b4", marker="o", label="this renderer"),
              "nee":  dict(color="#d62728", marker="s", label="corrected NEE\n(volprim + our fix)")}
    for arm, rows in data.items():
        if not rows:
            continue
        spp = np.array([r[0] for r in rows], float)
        err = np.array([r[1] for r in rows], float)  # corrected rel RMSE
        ax.loglog(spp, 100 * err, **styles[arm], ms=7, lw=1.8)
    # -1/2 slope guide anchored to the lowest-spp ours point available
    anchor = None
    for arm in ("ours", "nee"):
        if data[arm]:
            anchor = data[arm][0]; break
    if anchor:
        s0, e0 = anchor[0], anchor[1]
        gx = np.array([16, 4096], float)
        ax.loglog(gx, 100 * e0 * np.sqrt(s0 / gx), "k--", lw=1, alpha=0.6, label=r"$-1/2$ slope")
    ax.set_xlabel("samples per pixel"); ax.set_ylabel("relative RMSE to ground truth (%)")
    pass
    ax.grid(True, which="both", alpha=0.25); ax.legend(fontsize=8, framealpha=0.9)
    fig.tight_layout()
    fig.savefig(f"latex/figures/nee_convergence.pdf"); fig.savefig(f"latex/figures/nee_convergence.png", dpi=140)
    print(f"\nwrote {OUT}/convergence.{{pdf,png,csv}}")

if __name__ == "__main__":
    main()
