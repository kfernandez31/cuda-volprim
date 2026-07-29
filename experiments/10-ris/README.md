# 10 — Volumetric product-RIS (thesis Figs 6.3/6.4, §6.4)

**Claim.** Product-RIS direct lighting (K=6, reservoir, single shadow ray) is a
scene-dependent win: 1.48x at equal quality under peaky environment lighting; the win
decomposes into one-shadow-ray-instead-of-two (-21 % time) plus resampling (-14 % noise
constant); on flat environments it inverts (K-sweep, three peakiness levels).

**Run.**
```
build/bin/Release/test_runner --scene cloud_asset_scattering --ris --ris-candidates 6 ...
# A/B + K-sweep runners per the figure captions; plots:
python3 scripts/plots/ris_noise.py; python3 scripts/plots/figure_from_csv.py (ksweep)
```

**Expected.** Per-sample RMSE 0.160±0.0004 (RIS) vs 0.173±0.0006 (MIS), 16 seeds;
equal-quality 1.48x meadow; K=1 anchor already 1.19x on time. `[gpu][clocks]`.
