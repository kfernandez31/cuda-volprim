# 05 — Pixel-level cross-renderer agreement (thesis Fig 5.5, §5.5)

**Claim.** Under both direct-lighting estimators and both lighting regimes the two
implementations agree at the Monte-Carlo noise level, pixel by pixel; the only structured
residual traces to the reference's own shadow-march early-out (disabling it closes every
affected tile to within 1 %).

**Run.**
```
# arms and tiles: scripts/campaign/run_nee_fair_gate.sh + nee_fair panels scripts
python3 scripts/plots/nee_fair_panels.py
```

**Expected.** Meadow panel mean |Δ| 0.69 %, constant-env panel 0.07 %; deep-shadow tiles
show a 7–8 % deficit that vanishes with the reference's early-out disabled.
`[gpu][mitsuba]`, seeds as stated in the figure caption.
