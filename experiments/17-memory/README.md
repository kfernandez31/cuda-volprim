# 17 — Device memory (thesis Tab 7.3, §7.3)

**Claim.** Peak device memory is dominated by the per-ray reservation (caps), not scene
size; calibrated caps put the showcase at 578 vs the reference's 838 MiB; suite-wide
results vary and are reported in full.

**Run.**
```
bash scripts/campaign/run_g5b_vram.sh     # per-process nvidia-smi peaks, both renderers
```

**Expected.** Tab 7.3 rows: calibrated 578/818/600/900 MiB vs reference 838/806/806/806;
safe build flat 1200 MiB. Measurement asymmetry (per-process vs whole-GPU for the
reference) is documented in the table caption.
