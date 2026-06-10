# Campaign results → figures

Drop the experiment outputs here, then run `scripts/plots/build_figures.sh` to (re)generate
every campaign figure into `thesis/latex/figures/`, and `latexmk` the thesis. The figure floats are
already wired in the chapters; only the included PDFs change. All reported numbers must come from the
**full-blast** operating point (3090 at full clock).

## CSV-driven plots

| Figure (label) | CSV | Columns |
|---|---|---|
| `fig:rr-depth` (Ch 6) | `rr_depth.csv` | `rr_depth, frame_ms, k, eff` (plot `eff` = k·t, not frame_ms) |
| `fig:ris-ksweep` (Ch 6) | `ris_ksweep.csv` | `K, speedup_flat, speedup_studio, speedup_meadow` (peakiness ladder) |
| `fig:gas-memory` (Ch 6) | `gas_memory.csv` | `asset, gas_mb_uncompacted, gas_mb_compacted` (the **IAS** footprint; the GAS is a fixed unit sphere). **cloud measured at 150 W (clock-independent → final); bunny pending the asset-render path.** Figure held as placeholder until ≥2 assets. |

A CSV with only its header (the committed state) renders a watermarked **PROVISIONAL** placeholder; add
data rows and rerun to get the real plot. Schema details are in each CSV's header comments.

## Render montages (image, not CSV)

These three are assembled from rendered images and stay placeholders until the renders exist:

| Figure (label) | Output PDF | Feeding renders |
|---|---|---|
| `fig:absorption-ladder` (Ch 5) | `figures/absorption_ladder.pdf` | single / overlap / cloud, renderer vs analytic, per-scene RMSE |
| `fig:scattering-ladder` (Ch 5) | `figures/scattering_ladder.pdf` | renderer vs Mitsuba-analog across the ladder; converged-mean difference |
| `fig:showcase` (Ch 5) | `figures/showcase.pdf` | the cloud + meadow money shot, renderer vs Mitsuba-analog at equal quality; firefly crop |

To finalise a montage: assemble the renders (e.g. with ImageMagick `montage`) into the named PDF in
`thesis/latex/figures/`, overwriting the placeholder. No `.tex` edit is needed.
