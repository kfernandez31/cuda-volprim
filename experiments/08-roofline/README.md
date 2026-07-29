# 08 — Roofline placement (thesis Fig 6.1, §6.1)

**Claim.** The render megakernel sits far below both roofs on all four production assets
(0.2–3 % of FP32 peak, 0.6–16.5 % of bandwidth): latency-bound, divergence-dominated.

**Run.**
```
bash scripts/campaign/run_g4_suite_ncu.sh    # Nsight Compute captures, base clocks [gpu]
python3 scripts/plots/roofline.py --csv results/campaign/roofline.csv
```

**Expected.** Cloud megakernel ~31.2 % occupancy, 45.9 % SM, 16.5 % DRAM; 5.4–6.9/32
lanes active; bunny 20.9 % occupancy, no-eligible-warp 70 %. Single captures (kernels
deterministic; utilisation picture reproducible, exact percentages are one capture).
