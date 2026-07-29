#!/usr/bin/env python3
"""Task 2 — noise constant k = var * spp (per-sample variance), ours-MIS vs corrected-NEE, meadow.
Mirrors the 59x equal-quality methodology: per-pixel inter-seed variance (ddof=1), averaged over
pixels+channels, x spp -> k. Reported RAW and CLIPPED (99.9th pct radiance clamp, firefly suppression),
with an INDEPENDENT-per-arm bootstrap CI on the ratio (the convention fixed in g1_ci_bootstrap_resolution).

Arms (both 16 seeds x 64 spp, same showcase cloud/meadow scene):
  ours-MIS   : results/campaign/g1_seeds/cuda_seed{00..15}.exr      (banked, reused)
  corr-NEE   : results/campaign/nee_fair/ladder/gabor_nee_meadow_spp64_seed{0..15}.exr

Sanity: corrected-NEE raw ~ clipped (few fireflies), unlike Mitsuba-analog (raw >> clipped).

Run under the pip-gabor venv:
  experiments/mitsuba-reference/with_pip_gabor.sh experiments/mitsuba-reference/.venv/bin/python scripts/plots/nee_fair_k.py
"""
import glob, os
import numpy as np
import mitsuba as mi
mi.set_variant("scalar_rgb")

SPP = 64
OUT = "results/campaign/nee_fair"

def load(p):
    return np.array(mi.Bitmap(p)).astype(np.float64)[..., :3]

def stack(pattern, seeds=None):
    if seeds is None:
        files = sorted(glob.glob(pattern))
    else:
        files = [pattern % s for s in seeds]
    files = [f for f in files if os.path.exists(f)]
    assert files, f"no files for {pattern}"
    return np.stack([load(f) for f in files]), files          # (n,H,W,3)

def k_stack(s):
    """k = mean_{pix,ch} Var_seeds(ddof=1) * SPP for an already-(un)clipped seed stack (direct; slow)."""
    return float(s.var(axis=0, ddof=1).mean()) * SPP

def sufficient_stats(s):
    """Reduce a (n,H,W,3) seed stack to per-seed sums S_i and the seed x seed Gram matrix C_ij, both
    = mean over pixels+channels. Then for ANY resample idx of the n seeds:
        mean_{pix}[ Var_seeds(ddof=1) ] = (1/(n-1)) * ( sum_k S[idx_k] - (1/n) * sum_{k,l} C[idx_k,idx_l] )
    so each bootstrap iteration is O(n^2)=256 ops instead of O(26M). Exact, not an approximation."""
    n = s.shape[0]
    f = s.reshape(n, -1).astype(np.float64)     # (n, H*W*3)
    m = f.shape[1]
    S = (f * f).sum(1) / m                        # per-seed mean_pix[A_i^2]   (n,)
    C = (f @ f.T) / m                             # mean_pix[A_i . A_j]        (n,n)
    return S, C

def k_from_stats(S, C, idx, n):
    tot_S = S[idx].sum()
    tot_C = C[np.ix_(idx, idx)].sum()
    return SPP * (tot_S - tot_C / n) / (n - 1)

def main():
    ours, of = stack("results/campaign/g1_seeds/cuda_seed%02d.exr", range(16))
    nee,  nf = stack(f"{OUT}/ladder_fix5/gabor_nee_meadow_spp64_seed%d.exr", range(16))
    n = 16
    print(f"ours-MIS  : {len(of)} seeds  (means lum {np.mean([(x@[.2126,.7152,.0722]).mean() for x in ours]):.4f})")
    print(f"corr-NEE  : {len(nf)} seeds  (means lum {np.mean([(x@[.2126,.7152,.0722]).mean() for x in nee]):.4f})")

    # Clip threshold computed ONCE per arm (fixed firefly cap, 99.9th pct over all pixels/channels/seeds).
    stacks  = {"ours": ours, "nee": nee}
    clipped = {name: np.minimum(s, np.percentile(s, 99.9)) for name, s in stacks.items()}
    stats = {(name, kind): sufficient_stats(src[name])
             for name in ("ours", "nee") for kind, src in (("raw", stacks), ("clip", clipped))}

    ident = np.arange(n)
    res = {}
    for name in ("ours", "nee"):
        res[name] = {}
        for kind in ("raw", "clip"):
            S, C = stats[(name, kind)]
            k_fast = k_from_stats(S, C, ident, n)
            res[name][kind] = k_fast
        # validate the fast identity against the direct computation (once, on raw)
        k_direct = k_stack(stacks[name])
        assert abs(k_direct - res[name]["raw"]) < 1e-6 * max(1, abs(k_direct)), \
            f"{name}: fast {res[name]['raw']} != direct {k_direct}"
        print(f"\n{name}:  k_raw = {res[name]['raw']:.4f}   k_clip999 = {res[name]['clip']:.4f}   "
              f"(raw/clip = {res[name]['raw']/res[name]['clip']:.2f})   [fast==direct OK]")

    for kind in ("raw", "clip"):
        ratio = res["nee"][kind] / res["ours"][kind]
        print(f"\n=== k ratio ({kind}): corrected-NEE / ours = {ratio:.3f} "
              f"({'NEE noisier' if ratio>1 else 'ours noisier'} per sample) ===")

    # Independent per-arm bootstrap CI on k_nee/k_ours (both kinds), O(n^2) per iter via sufficient stats.
    rng = np.random.default_rng(0)
    B = 2000
    for kind in ("raw", "clip"):
        So, Co = stats[("ours", kind)]
        Se, Ce = stats[("nee", kind)]
        boots = np.empty(B)
        for b in range(B):
            io = rng.integers(0, n, n); ie = rng.integers(0, n, n)
            boots[b] = k_from_stats(Se, Ce, ie, n) / k_from_stats(So, Co, io, n)
        lo, hi = np.percentile(boots, [2.5, 97.5])
        pt = res["nee"][kind] / res["ours"][kind]
        print(f"  bootstrap k_nee/k_ours ({kind}, B={B}, independent arms): "
              f"{pt:.3f}  95% CI [{lo:.3f}, {hi:.3f}]")

    with open(f"{OUT}/k_table_fix5_n16.csv", "w") as fh:
        fh.write("arm,k_raw,k_clip999\n")
        fh.write(f"ours,{res['ours']['raw']:.5f},{res['ours']['clip']:.5f}\n")
        fh.write(f"nee,{res['nee']['raw']:.5f},{res['nee']['clip']:.5f}\n")
    print(f"\nwrote {OUT}/k_table_fix5_n16.csv")

if __name__ == "__main__":
    main()
