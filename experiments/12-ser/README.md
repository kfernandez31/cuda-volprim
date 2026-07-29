# 12 — Shader Execution Reordering (thesis Tabs 6.5/6.6, §6.6)

**Claim.** One optixReorder per bounce with a scatter-cell coherence key yields
1.12–1.68x at equal quality, image-identical, on Ada hardware (the RTX 3090 used
everywhere else cannot execute SER).

**Run.** `[ada][clocks]` — requires an Ada GPU (thesis used a rented RTX 4090).
```
cmake -DTHESIS_ENABLE_SER=ON ...   # per-bounce reorder + scatter-cell key
# per-asset A/B at calibrated caps, 64 spp, 11 interleaved repetitions
```

**Expected.** cloud 1.42x, explosion 1.41x, tornado 1.12x, bunny 1.68x; SER-on images
bit-identical to SER-off; hintless reorder and bounce-0-only ablations per Tab 6.5's
caption. NOT reproducible on Ampere — documented, not pretended otherwise.
