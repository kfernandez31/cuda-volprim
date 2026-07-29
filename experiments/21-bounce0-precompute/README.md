# 21 — Bounce-0 camera-set precompute (thesis §7.6 close, §8.3; post-freeze)

**Claim.** Precomputing the camera-origin containment set once per render (perspective
cameras share one origin) removes the renderer's one remaining O(N) per-sample term.
Output is byte-identical to stock; implemented AFTER the measurement freeze on branch
`feature/bounce0-camera-set` — production numbers in the thesis are measured WITHOUT it.

**Run.**
```
git switch feature/bounce0-camera-set
bash scripts/campaign/build_b0opt_pairs.sh   # 8 binaries: {stock,opt} x 4 assets
bash scripts/campaign/run_b0opt_ab.sh        # locked clocks, bit-compare gate [clocks]
```

**Expected.** Seed-0 images identical per asset (gate column = 1); speedups per the
thesis's post-freeze note (model predicted ~5 % cloud / ~12 % bunny; measured values
quoted in §7.6 once the controlled A/B is in the text).
