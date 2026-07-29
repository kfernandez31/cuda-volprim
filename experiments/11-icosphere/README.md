# 11 — Analytic sphere vs tessellated icosphere (thesis Tab 6.4 / Fig 6.5, §6.5)

**Claim.** The tessellated shell is faster at every level (analytic sphere pays
1.17–1.58x for exactness), but accuracy is non-monotone: the l=3 shell shows a
silhouette-localised sliver artefact (deterministic, seed-free), so the analytic shell
stays the production path.

**Run.**
```
# build per level: cmake -DTHESIS_ICOSPHERE=ON -DTHESIS_ICOSPHERE_N=<l>
bash scripts/campaign/run_icosphere_outline_test.sh
bash scripts/campaign/run_icosphere_accuracy_v2.sh
python3 scripts/plots/icosphere_sliver.py
```

**Expected.** Tab 6.4 timing ratios per level; l=3 error rise reproduces exactly
(deterministic absorption); sliver localised at the silhouette, sign-flipped.
