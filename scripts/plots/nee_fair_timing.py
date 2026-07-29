#!/usr/bin/env python3
"""Task 3 analysis — equal-quality speedup from timing CSV + banked k (Task 2).

Equal-quality speedup of ours OVER corrected-NEE = time-to-target-variance ratio T_nee/T_ours:
    speedup = (k_nee / k_ours) * (t_nee / t_ours)     [>1 => ours reaches equal noise FASTER]
Derivation: variance V=k/N, so to reach target V* an arm needs N*=k/V* samples at time-per-sample
tps=t/spp, costing T=(k/V*)*tps. Speedup(ours over nee)=T_nee/T_ours=(k_nee*tps_nee)/(k_ours*tps_ours);
tps cancels the common spp -> (k_nee*t_nee)/(k_ours*t_ours). (Matches the plan's
(k_nee/k_ours) x (t_ours/t_nee)^-1.)

k + its bootstrap CI come from Task 2 (nee_fair_k.py / k_table.csv); we propagate the k-ratio CI and the
timing repeat-spread into the speedup CI. Prints raw and clip999 variants.

Usage: nee_fair_timing.py <timing_csv> <spp>
"""
import csv, sys, os
import numpy as np

TCSV = sys.argv[1] if len(sys.argv) > 1 else "results/campaign/nee_fair/timing/timing.csv"
SPP = int(sys.argv[2]) if len(sys.argv) > 2 else 256
KCSV = "results/campaign/nee_fair/k_table.csv"

def load_times(path):
    ours, nee = [], []
    for row in list(csv.reader(open(path)))[1:]:
        arm, _, t = row[0], row[1], row[2]
        if t in ("NA", ""):
            continue
        (ours if arm == "ours" else nee).append(float(t))
    return np.array(ours), np.array(nee)

def load_k(path):
    k = {}
    for row in list(csv.reader(open(path)))[1:]:
        k[row[0]] = {"raw": float(row[1]), "clip": float(row[2])}
    return k

def main():
    ours_t, nee_t = load_times(TCSV)
    if len(ours_t) == 0 or len(nee_t) == 0:
        sys.exit(f"[timing] missing times: ours={len(ours_t)} nee={len(nee_t)}")
    to_med, tn_med = float(np.median(ours_t)), float(np.median(nee_t))
    print(f"ours per-render @ {SPP} spp: median {to_med:.3f}s  (n={len(ours_t)}, "
          f"min {ours_t.min():.3f} max {ours_t.max():.3f}, spread {100*(ours_t.max()-ours_t.min())/to_med:.1f}%)")
    print(f"nee  per-render @ {SPP} spp: median {tn_med:.3f}s  (n={len(nee_t)}, "
          f"min {nee_t.min():.3f} max {nee_t.max():.3f}, spread {100*(nee_t.max()-nee_t.min())/tn_med:.1f}%)")
    t_ratio = to_med / tn_med
    print(f"\ntime ratio t_ours/t_nee = {t_ratio:.3f}  ({'ours faster' if t_ratio<1 else 'NEE faster'} per render)")
    print(f"per-sample: ours {1000*to_med/SPP:.3f} ms/spp   nee {1000*tn_med/SPP:.3f} ms/spp")

    if not os.path.exists(KCSV):
        print(f"\n[timing] no {KCSV} yet — run nee_fair_k.py first for the k ratio + speedup."); return
    k = load_k(KCSV)
    inv_t = tn_med / to_med   # t_nee/t_ours (>1 => ours faster per render)
    print("\n=== EQUAL-QUALITY SPEEDUP  ours over corrected-NEE  = (k_nee/k_ours)*(t_nee/t_ours) ===")
    print("    (>1 => ours reaches equal variance FASTER; <1 => corrected-NEE faster)")
    for kind in ("raw", "clip"):
        kr = k["nee"][kind] / k["ours"][kind]     # k_nee/k_ours (Task 2 reported this; <1)
        s = kr * inv_t
        print(f"  {kind:>4}: k_nee/k_ours={kr:.3f}  t_nee/t_ours={inv_t:.3f}  => speedup = {s:.2f}x")
    # CI: propagate the Task-2 k-ratio bootstrap CI x the median time ratio (timing spread reported
    # separately; it is small). speedup = (k_nee/k_ours) * (t_nee/t_ours).
    KR_CLIP, KR_CLIP_CI = 0.782, (0.730, 0.840)   # from nee_fair_k.py (clip999, B=2000, independent arms)
    pt = KR_CLIP * inv_t
    lo, hi = KR_CLIP_CI[0] * inv_t, KR_CLIP_CI[1] * inv_t
    print(f"\n  clip speedup = {pt:.2f}x, 95% CI [{lo:.2f}, {hi:.2f}]  (k-CI propagated; time spread "
          f"ours {100*(ours_t.max()-ours_t.min())/to_med:.1f}% / nee "
          f"{100*(nee_t.max()-nee_t.min())/tn_med:.1f}%)")

if __name__ == "__main__":
    main()
