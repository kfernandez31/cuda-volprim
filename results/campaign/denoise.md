# Denoiser — provenance for fig:denoise + the effective-spp claim

**fig:denoise** (the figure): OptiX denoiser on a **16-spp** cloud--meadow render vs a **1024-spp**
reference. Image-space RMSE **0.353 -> 0.049 (7.2x)**. Source EXRs: `denoise/*.exr` — reproduce by
recomputing RMSE from the noisy / denoised / reference frames. (This is the qualitative illustration.)

**Effective-spp claim (~28x)** — the quantitative anchor, from the controlled sweep in `g2_denoiser.md`:
denoised 64-spp RMSE **0.0337** vs uniform 64-spp **0.1783**, both against a **2048-spp** GT; the
denoised-64 frame beats a **1024-spp** uniform render (0.054). Fitting uniform RMSE ∝ 1/√spp gives an
effective `64·(0.178/0.034)^2 ≈ 1788 spp ≈ 28x`.

The thesis cites the **~28x** controlled-sweep figure, NOT the 16-spp figure's own
`(0.353/0.049)^2 ≈ 52x` (which the 1/√spp extrapolation would over-state for a single render).
