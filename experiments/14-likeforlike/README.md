# 14 — Headline equal-quality comparison (thesis Fig 7.1, §7.1)

**Claim.** Against the reference's corrected next-event estimator, this renderer's MIS
is 2.72x faster at equal quality (95 % CI [2.54, 2.92]; raw basis 2.67x). Per sample the
corrected reference is LESS noisy (k-ratio 0.785 in its favour); the win is per-second.

**Run.**
```
bash scripts/campaign/run_nee_fair_ladder.sh    # variance arms, 16 seeds @64spp anchor
bash scripts/campaign/run_nee_fair_timing.sh    # locked clocks, interleaved  [clocks]
python3 scripts/plots/nee_fair_k.py; python3 scripts/plots/likeforlike_equalvar.py
```

**Expected.** k_nee/k_ours = 0.785 [0.734, 0.842] (2000 bootstrap resamples, per-arm
index sets); times 120.4 vs 417.2 ms/sample at 256 spp; net 2.72x [2.54, 2.92] clipped,
2.67x raw. Requires the corrected fork (`mitsuba-reference/`). `[gpu][clocks][mitsuba]`
