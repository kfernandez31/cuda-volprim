# 04 — Error-to-ground-truth convergence (thesis Fig 5.4, §5.5)

**Claim.** Both this renderer's MIS estimator and the reference's corrected NEE follow
the exact -1/2 Monte-Carlo slope from 16 to 4096 spp with no error floor; measured error
matches the independent inter-seed noise estimate at every budget (unbiasedness evidence).

**Run.**
```
bash scripts/campaign/run_cloud_convergence.sh    # ours arm [gpu]
# reference arm: mitsuba-reference harness (corrected fork)
python3 scripts/plots/nee_convergence_thesis.py
```

**Expected.** Relative RMSE falls 106.6 % -> 6.6 % (ours), 92.8 % -> 5.8 % (reference);
every step ratio within 1 % of the ideal -1/2 slope; GT-noise-corrected curves keep
falling (no floor). 4 seeds per budget, GT seeds disjoint.
