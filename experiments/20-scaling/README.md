# 20 — Scaling with primitive count (thesis Fig 7.4 / Tab 7.4, §7.6)

**Claim.** Render time follows what rays cross, not the scene total: three constructed
families (sheet/cube/stack) pin crossings to constant / N^(1/3) / N; stack slope 1.01;
sheet flat ±2 % over 16x; the residual linear term is the bounce-0 containment scan
(~0.26 ms/primitive at 512x512x64spp, ~20x cheaper than transport).

**Run.**
```
python3 scripts/tools/gen_scaling_v2.py         # 52 synthetic PLYs (in-repo generator)
bash scripts/campaign/run_scaling_v2.sh # locked clocks; resumable      [clocks]
experiments/mitsuba-reference/.venv NOT required; plot: scripts/plots/scaling_v2.py
```

**Expected.** scaling_v2.csv: 52 rows; hit counters 4 / 4n / N at every size
(scaling_v2_caps.csv); joint model median error 12 %; spp/resolution decomposition at
N=8281 scales with samples and rays, not crossings. Absorption = deterministic.
