#!/usr/bin/env python3
"""Cutoff-bias probe analysis: is the ~5% dense-core ours-vs-correctedNEE gap explained by the
reference's total_tr>0.001 march early-out (Jorge's 'TODO this is biased')?

Inputs (same seed/spp for cut3 vs cut7 => their diff is the exact cutoff effect on identical paths):
  cut3  = corrected fork, cutoff 0.001  (results/campaign/nee_fair/cutoff_probe/cut3/*.exr)
  cut7  = corrected fork, cutoff 1e-7   (results/campaign/nee_fair/cutoff_probe/cut7_spp64_seed0.exr)
  oursG = ours-MIS GT   (mean of results/campaign/g1_seeds/cuda_seed*.exr)
  neeG  = corrected-NEE GT, cutoff 0.001 (mean of results/campaign/nee_fair/gt/*.exr)

Readouts: per-60px-block luminance maps; (cut7-cut3)/cut3 signed field; block-level correlation and
magnitude match against the gate's (oursG-neeG)/neeG field.
"""
import glob
import numpy as np
import mitsuba as mi
mi.set_variant("scalar_rgb")

def load(path):
    return np.array(mi.Bitmap(path), dtype=np.float64)

def load_mean(pattern):
    fs = sorted(glob.glob(pattern))
    assert fs, pattern
    return np.mean([load(f) for f in fs], axis=0), len(fs)

LUM = np.array([0.2126, 0.7152, 0.0722])
def blocks(img, B=60):
    g = img @ LUM
    h, w = g.shape
    return g[:h // B * B, :w // B * B].reshape(h // B, B, w // B, B).mean((1, 3))

cut3, _ = load_mean("results/campaign/nee_fair/cutoff_probe/cut3/*.exr")
cut7 = load("results/campaign/nee_fair/cutoff_probe/cut7_spp64_seed0.exr")
oursG, n_ours = load_mean("results/campaign/g1_seeds/cuda_seed*.exr")
neeG, n_nee = load_mean("results/campaign/nee_fair/gt/gabor_nee_meadow_spp2048_seed*.exr")
print(f"loaded: ours GT x{n_ours}, nee GT x{n_nee}, cut3/cut7 64spp seed0")
print(f"means: cut3={np.mean(cut3 @ LUM):.4f} cut7={np.mean(cut7 @ LUM):.4f} "
      f"ours={np.mean(oursG @ LUM):.4f} neeGT={np.mean(neeG @ LUM):.4f}")

b3, b7, bo, bn = blocks(cut3), blocks(cut7), blocks(oursG), blocks(neeG)
eps = 1e-4
cut_eff = (b7 - b3) / np.maximum(b3, eps)          # cutoff effect (relaxing it), signed
gate_gap = (bo - bn) / np.maximum(bn, eps)         # the gate's ours-vs-NEE gap, signed

print("\nblock@60 fields (10x15):")
print(f"  cutoff effect (cut7-cut3)/cut3: min {cut_eff.min()*100:+.2f}%  max {cut_eff.max()*100:+.2f}%")
print(f"  gate gap (ours-neeGT)/neeGT   : min {gate_gap.min()*100:+.2f}%  max {gate_gap.max()*100:+.2f}%")

# where is the gate gap worst (the dense core)? does the cutoff effect sit there with matching sign+size?
i, j = np.unravel_index(np.argmin(gate_gap), gate_gap.shape)   # most-negative = ours darkest vs NEE
print(f"\nworst gate-gap block ({i},{j}): gate {gate_gap[i,j]*100:+.2f}%  cutoff-effect {cut_eff[i,j]*100:+.2f}%")

# correlation over blocks where the medium is present (luminance above background floor)
mask = bn > 0.05
r = np.corrcoef(cut_eff[mask], gate_gap[mask])[0, 1]
print(f"correlation(cutoff-effect, gate-gap) over {mask.sum()} medium blocks: r = {r:+.3f}")

# residual gap if the reference had used the relaxed cutoff: ours vs (neeGT + cutoff effect)
resid = (bo - bn * (1 + cut_eff)) / np.maximum(bn * (1 + cut_eff), eps)
print(f"\nresidual gap after applying cutoff-effect to the reference:")
print(f"  worst block was {gate_gap[i,j]*100:+.2f}%  ->  {resid[i,j]*100:+.2f}%")
print(f"  max |gap| overall: {np.abs(gate_gap[mask]).max()*100:.2f}%  ->  {np.abs(resid[mask]).max()*100:.2f}%")
