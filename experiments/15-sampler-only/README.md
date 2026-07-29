# 15 — Sampler-only comparison, flat environment (thesis §7.1 "Isolating the sampler")

**Claim.** Both renderers in analog mode (no NEE), flat environment, matched box filter:
the argmin sampler is 1.6x noisier per sample but 4.6x faster per sample — net 2.87x at
equal quality (power-dependent; rises at lower caps).

**Run.**
```
bash scripts/campaign/run_analog_convergence.sh   # both arms, 16..512 spp, 8 seeds
bash scripts/campaign/run_flat_timing_pin.sh      # locked clocks               [clocks]
```

**Expected.** Variance ratio 1.59 constant across the spp sweep; converged means agree
to 0.02 %; net 2.87x at 350 W. The withdrawn mismatched-filter reading (0.83x) is
documented in the measurement record — reproduce only with matched filters.
