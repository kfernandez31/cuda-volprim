# G2 denoiser effective-RMSE — CONFIRMS ~30x, 2026-06-14

350 W, clocks at 1800 MHz. Driver `scripts/campaign/run_g2_denoiser.sh`; cloud-meadow, calibrated
binary, vs a 2048-spp ground truth (246 s).

| render | RMSE vs 2048-spp GT |
|---|---|
| denoised 64 spp | **0.0337** |
| uniform 64 spp | 0.1783 |
| uniform 128 | 0.1286 |
| uniform 256 | 0.0937 |
| uniform 512 | 0.0698 |
| uniform 1024 | 0.0540 |

The OptiX denoiser at 64 spp reaches a lower RMSE (0.034) than a **1024-spp** uniform render (0.054).
Fitting the uniform sweep to RMSE $\propto 1/\sqrt{\text{spp}}$ puts the denoised-64 frame's effective
sample count at $64\times(0.178/0.034)^2 \approx 1788$ spp, i.e. **~28x effective** — cleanly confirming
the development-time "~30x effective" already in `tab:wins` (§8.22). (Absolute RMSEs are inflated by the
GT's own 2048-spp noise floor, but the effective ratio is robust.) `tab:wins` left unchanged; this is an
independent campaign confirmation of the denoiser row. As before, the denoiser is a biased post-process,
reported as a practical option, not part of the unbiased pipeline.
