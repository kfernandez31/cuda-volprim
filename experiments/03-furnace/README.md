# 03 — Furnace test, four estimator arms (thesis Fig 5.3, §5.5)

**Claim.** In an albedo-1 medium inside a unit-radiance environment every pixel of a
correct render equals exactly 1. Both of this renderer's estimators and both corrected
reference arms sit on zero over-count at every sample count; the analog arms are exact
zeros with no spread (an analog furnace path never changes weight).

**Run.**
```
bash scripts/campaign/run_furnace_bank.sh        # ours-MIS + ours-analog arms [gpu]
# corrected-fork NEE/analog arms: mitsuba-reference/README.md (fork + fix PR)
python3 scripts/plots/furnace_four.py --out furnace_four.pdf
```

**Expected.** Centre over-count 0 within 95% t-intervals over 8 seeds at every spp
(64..4096), density scales 6 and 12; analog arms exactly 1.0 per sample. The SHIPPED
reference NEE arm (not plotted) shows +9.7 % / +31 % over-count at the same settings —
the §7.2 story. Data layout: furnace4.csv (arm,sigma,spp,seed,centre).
