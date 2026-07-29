# 19 — Cross-renderer generalisation (thesis Fig 7.3, §7.5)

**Claim.** On three further DSYG assets (tornado, explosion, bunny) the two renderers
agree: vs Mitsuba-analog within 0.16 % (four digits per asset); under peaky meadow vs
corrected NEE, mean ratios 0.9952/0.9988/0.9958 with speckle-only difference maps.

**Run.**
```
bash scripts/campaign/run_gen_topup.sh          # meadow arms at matched budgets
bash scripts/campaign/run_g10_parity.sh         # analog gates
python3 scripts/plots/generalisation_thesis.py
```

**Expected.** Ratios above ±SE ~1e-4-class; equal 4096-spp effective budgets both arms
(ours 16x256, reference 4x1024). `[gpu][mitsuba]`
