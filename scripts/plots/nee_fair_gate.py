#!/usr/bin/env python3
"""Gate G1 — pixelwise cross-renderer ground-truth agreement (Task 0 of the fair-NEE campaign).

Compares three independent estimates of the SAME showcase image (cloud, meadow, sigma7.5, albedo0.9,
HG0.85, cam_0000, box filter):
  - ours-MIS  : our CUDA test_runner, NEE+MIS   (avg of results/campaign/g1_seeds/cuda_seed*.exr)
  - nee-fixed : volprim NEE WITH OUR CORRECTION (avg of the corrected-NEE GT seeds, gabor_cloud.py)
  - analog    : Mitsuba's own unbiased mode      (avg of results/campaign/g1_seeds/mits_seed*.exr) [context]

The headline gate is ours-MIS vs nee-fixed: two different renderers AND two different estimators. If they
agree pixelwise, the upstream-reported correction is validated cross-renderer at pixel level, which is the
prerequisite for every downstream comparative claim.

PASS: relative RMSE (normalized by reference mean) <= ~2% AND mean ratio in [0.99, 1.01].

Usage (under the pip-gabor venv so mitsuba.Bitmap can read the EXRs):
  experiments/mitsuba-reference/with_pip_gabor.sh experiments/mitsuba-reference/.venv/bin/python scripts/plots/nee_fair_gate.py
"""
import glob, os, sys
import numpy as np
import mitsuba as mi
mi.set_variant("scalar_rgb")

OURS_GLOB   = "results/campaign/g1_seeds/cuda_seed*.exr"
NEE_GLOB    = "results/campaign/nee_fair/gt/gabor_nee_meadow_spp2048_seed*.exr"
ANALOG_GLOB = "results/campaign/g1_seeds/mits_seed*.exr"
OUTDIR      = "results/campaign/nee_fair"
LUM_W       = np.array([0.2126, 0.7152, 0.0722], np.float32)  # Rec.709
THRESH      = 1e-3   # luminance mask floor

def load_avg(pattern):
    files = sorted(glob.glob(pattern))
    if not files:
        sys.exit(f"[gate] no EXRs match {pattern}")
    imgs = [np.array(mi.Bitmap(f)).astype(np.float64)[..., :3] for f in files]
    shapes = {im.shape for im in imgs}
    if len(shapes) != 1:
        sys.exit(f"[gate] mismatched shapes for {pattern}: {shapes}")
    return np.mean(imgs, axis=0), files

def lum(rgb):
    return rgb @ LUM_W

def metrics(a, b, mask):
    """a vs reference b, restricted to mask. Returns dict of agreement metrics (luminance-based)."""
    la, lb = lum(a), lum(b)
    am, bm = la[mask], lb[mask]
    ref_mean = float(bm.mean())
    rmse = float(np.sqrt(np.mean((am - bm) ** 2)))
    return {
        "n_pixels":        int(mask.sum()),
        "mean_a":          float(am.mean()),
        "mean_b":          ref_mean,
        "mean_ratio":      float(am.mean() / ref_mean),          # a / b
        "rel_rmse_normed": rmse / ref_mean,                       # RMSE / mean(ref)   <- headline
        "rel_rmse_perpix": float(np.sqrt(np.mean(((am - bm) / np.maximum(bm, THRESH)) ** 2))),
        "max_abs_diff":    float(np.abs(am - bm).max()),
    }

def main():
    ours,   ours_f   = load_avg(OURS_GLOB)
    nee,    nee_f     = load_avg(NEE_GLOB)
    analog, analog_f  = load_avg(ANALOG_GLOB)
    print(f"[gate] ours-MIS   : {len(ours_f)} seeds  {os.path.basename(ours_f[0])} ...")
    print(f"[gate] nee-fixed  : {len(nee_f)} seeds  {[os.path.basename(f) for f in nee_f]}")
    print(f"[gate] analog     : {len(analog_f)} seeds  {os.path.basename(analog_f[0])} ...")

    if not (ours.shape == nee.shape == analog.shape):
        sys.exit(f"[gate] shape mismatch: ours{ours.shape} nee{nee.shape} analog{analog.shape}")

    # Mask on the reference (nee-fixed) luminance; the headline comparison is ours vs nee-fixed.
    mask = lum(nee) > THRESH
    print(f"\n[gate] resolution {ours.shape}, {int(mask.sum())}/{mask.size} pixels above lum {THRESH}")

    print("\n=== full-image means (all pixels, per-channel + luminance) ===")
    for name, im in [("ours-MIS", ours), ("nee-fixed", nee), ("analog", analog)]:
        r, g, b = im.reshape(-1, 3).mean(0)
        print(f"  {name:>10}: R{r:.4f} G{g:.4f} B{b:.4f}  lum{float(lum(im).mean()):.4f}")

    print("\n=== HEADLINE GATE: ours-MIS vs nee-fixed (reference = nee-fixed) ===")
    m = metrics(ours, nee, mask)
    for k, v in m.items():
        print(f"  {k:>16}: {v:.5f}" if isinstance(v, float) else f"  {k:>16}: {v}")

    print("\n=== context: nee-fixed vs analog (Mitsuba's own unbiased mode; analog is firefly-noisy) ===")
    ma = metrics(nee, analog, mask)
    for k in ("mean_ratio", "rel_rmse_normed"):
        print(f"  {k:>16}: {ma[k]:.5f}")
    print("\n=== context: ours-MIS vs analog ===")
    moa = metrics(ours, analog, mask)
    for k in ("mean_ratio", "rel_rmse_normed"):
        print(f"  {k:>16}: {moa[k]:.5f}")

    # Verdict on the headline gate.
    rrmse, ratio = m["rel_rmse_normed"], m["mean_ratio"]
    passed = (rrmse <= 0.02) and (0.99 <= ratio <= 1.01)
    print(f"\n{'='*60}")
    print(f"GATE G1 VERDICT: rel_rmse={rrmse*100:.3f}% (<=2%?)  mean_ratio={ratio:.4f} (in [0.99,1.01]?)")
    print(f"  => {'PASS' if passed else 'FAIL'}")
    print(f"{'='*60}")

    # |diff| EXR + relative-error map for eyeballing.
    os.makedirs(OUTDIR, exist_ok=True)
    diff = np.abs(ours - nee).astype(np.float32)
    mi.Bitmap(diff).write(os.path.join(OUTDIR, "gate_absdiff_ours_vs_neefixed.exr"))
    rel = (np.abs(lum(ours) - lum(nee)) / np.maximum(lum(nee), THRESH)).astype(np.float32)
    mi.Bitmap(np.stack([rel, rel, rel], -1)).write(os.path.join(OUTDIR, "gate_relerr_ours_vs_neefixed.exr"))
    print(f"[gate] wrote gate_absdiff / gate_relerr EXRs to {OUTDIR}/")
    return 0 if passed else 1

if __name__ == "__main__":
    sys.exit(main())
