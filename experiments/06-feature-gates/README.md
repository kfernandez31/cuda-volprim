# 06 — Feature-validation gates (thesis Tab 5.1, §5.6)

**Claim.** Each rendering feature (HG phase g=0.85, MIS, env-IS, RIS, firefly clamp,
orientation, coloured albedo) passes a per-feature gate against the reference or an
exact invariant; one readout per row.

**Run.** The G-campaign scripts reproduce individual rows:
```
scripts/campaign/run_g1*.sh   # phase/agreement rows   [gpu][mitsuba]
scripts/campaign/run_g10_parity.sh, run_g8_reanchor.sh, ...
```
Row-to-script mapping is in each script's header comment.

**Expected.** The table's readouts (e.g. HG furnace bias 0.5e-4; single-Gaussian median
|Δ| 2.4e-4 at 24 seeds; MIS furnace mean 1.00011). Tolerances are per-row (mean ± SE).
