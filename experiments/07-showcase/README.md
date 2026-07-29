# 07 — Combined showcase (thesis Fig 5.6, §5.7)

**Claim.** The full cloud under the measured meadow HDR renders firefly-free with MIS
at production budgets; ours-MIS vs ours-analog converge to the same image (fireflies are
an intra-renderer variance phenomenon, not a bias).

**Run.**
```
bash scripts/tools/fetch_envmaps.sh               # meadow_2_4k.hdr
build/bin/Release/test_runner --scene cloud_asset_scattering --spp 256 --seed 0 \
  --sigma-multiplier 7.5
python3 scripts/plots/showcase.py
```

**Expected.** MIS panel visually firefly-free at 256 spp; analog panel firefly-dominated
at equal budget; both means agree within noise (0.3215 pooled, ±1e-4 class).
