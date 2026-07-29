# 09 — Russian-roulette start depth (thesis Fig 6.2, §6.3)

**Claim.** k·t efficiency on the scattering cloud is optimal at RR start depth 12
(vs the conventional 5): deeper start pays more time but removes more variance.

**Protocol** (no standalone runner survives from the June campaign; the full record is
reproduced here). Cloud scattering, meadow env, SG_CAM=0, 64 spp, MIS (no RIS),
seeds 1–16, depths {5,6,8,10,12,16}, arms interleaved depth-within-seed-block,
locked clocks:
```
for seed in 1..16: for depth in 5 6 8 10 12 16:
  SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner \
    --scene cloud_asset_scattering --spp 64 --sigma-multiplier 7.5 \
    --seed $seed --rr-depth $depth
# k from per-depth seed stacks; t as per-block-normalised medians
python3 scripts/plots/figure_from_csv.py --csv rr_depth.csv ...
```

**Expected** (96 renders): eff = k·t_rel minimises at depth 12; e.g. depth 5:
k=2.461, eff 2.183 vs depth 12 minimum; t16/t5 = 1.30 ± 0.06. `[gpu][clocks]`
