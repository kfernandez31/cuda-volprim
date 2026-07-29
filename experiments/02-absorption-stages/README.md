# 02 — Absorption validation in three stages (thesis Fig 5.2, §5.4)

**Claim.** Single Gaussian matches its closed form to ~2e-5; overlapping pair and full
cloud match Mitsuba with residual RMSE 0.0007 / 0.0004 (reference arms 16384 / 49152 spp
effective); the cloud's silhouette band is settled by a float64 brute-force check
(T-RMSE 9.5e-5, mean optical-depth ratio 0.9999).

**Run.**
```
bash scripts/campaign/run_g2_ladder.sh          # ours + Mitsuba arms  [gpu][mitsuba]
python3 ../mitsuba-reference/cloud_bruteforce_transmittance.py   # float64 check (SS=4)
tools-free plot: scripts/plots/ladder_montage.py
```

**Expected.** Mean ratios 1.0000 (single, vs closed form), pair/cloud per-pixel RMSE
0.0007 / 0.0004 vs converged reference; brute force: mean-tau ratio 0.99990, median
relative tau error 0.046 %. Absorption arms are deterministic (no seeds).
Runtime: ours seconds; reference arms hours at high spp (banked values quoted in thesis).
