#!/usr/bin/env bash
# Regenerate every campaign figure into latex/figures/.
#
# Turnkey: drop real CSVs into results/campaign/ (schemas documented there) and rerun.
# A figure whose CSV has data becomes a real plot; otherwise it stays a watermarked
# "PROVISIONAL" placeholder. The figure floats in the thesis never change -- only the
# PDF they include. The three validation montages are assembled from renders, so they
# stay placeholders until those renders exist (see results/campaign/README.md).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"   # montage scripts (ladder, showcase, reduced_density) use repo-relative paths
PY="$ROOT/experiments/mitsuba-reference/.venv/bin/python"
FIG="$ROOT/latex/figures"
RES="$ROOT/results/campaign"
mkdir -p "$FIG"

plot() { "$PY" "$ROOT/scripts/plots/figure_from_csv.py" "$@"; }
ph()   { "$PY" "$ROOT/scripts/plots/placeholder.py" "$@"; }
has_data() { [ -f "$1" ] && [ "$(grep -vcE '^[[:space:]]*#|^[[:space:]]*$' "$1")" -gt 1 ]; }

# --- 1. RR-depth efficiency (Ch 6) ---
if has_data "$RES/rr_depth.csv"; then
  # Plot efficiency (k*t, lower = better) vs depth -- shows the knee. frame_ms alone is monotone.
  plot --csv "$RES/rr_depth.csv" --x rr_depth --y eff --xlabel "RR start depth" \
       --ylabel "efficiency k·t (lower is better)" --title "RR-depth efficiency" --out "$FIG/rr_depth.pdf"
else
  ph --title "RR-depth efficiency" --note "efficiency k*t vs RR start depth (knee near 12; 5 = old default)" \
     --out "$FIG/rr_depth.pdf"
fi

# --- 2. RIS K-sweep (Ch 6) ---
if has_data "$RES/ris_ksweep.csv"; then
  plot --csv "$RES/ris_ksweep.csv" --x K --y speedup_flat speedup_studio speedup_meadow \
       --xlabel "RIS candidates K" --ylabel "equal-quality speedup" --title "RIS vs MIS" \
       --hline 1.0 --out "$FIG/ris_ksweep.pdf"
else
  ph --title "RIS vs MIS, equal quality" --note "K-sweep across peakiness: flat / studio / meadow" \
     --out "$FIG/ris_ksweep.pdf"
fi

# --- 3. GAS compaction (Ch 6) ---
if has_data "$RES/gas_memory.csv"; then
  plot --csv "$RES/gas_memory.csv" --x asset --y gas_mb_uncompacted gas_mb_compacted --kind bar --logy \
       --xlabel "asset" --ylabel "acceleration-structure size (MB, log)" \
       --title "Acceleration-structure compaction" --out "$FIG/gas_memory.pdf"
else
  ph --title "GAS compaction" --note "acceleration-structure size before/after, per asset" \
     --out "$FIG/gas_memory.pdf"
fi

# --- 4. Roofline (Ch 6): non-saturation argument (point far under both roofs -> latency-bound) ---
if has_data "$RES/roofline.csv"; then
  "$PY" "$ROOT/scripts/plots/roofline.py" --csv "$RES/roofline.csv" --out "$FIG/roofline.pdf"
else
  ph --title "Roofline" --note "arithmetic intensity vs achieved GFLOP/s (non-saturation -> latency-bound)" \
     --out "$FIG/roofline.pdf"
fi

# --- 4b. Monte-Carlo convergence (Ch 2 background): self-contained, no CSV ---
"$PY" "$ROOT/scripts/plots/mc_integ.py" --out "$FIG/mc_integ.pdf"
"$PY" "$ROOT/scripts/plots/scaling_v2.py" --csv "$ROOT/results/campaign/scaling_v2.csv" --out "$FIG/scaling_v2.pdf"

# --- 5-7. Validation montages (Ch 5): assembled from banked renders when present,
# placeholder otherwise (a fresh clone has no results/, so it gets placeholders) ---
if [ -f "$RES/ladder/abs_single_ours.exr" ]; then
  "$PY" "$ROOT/scripts/plots/ladder_montage.py" absorption "$FIG/absorption_ladder.pdf"
else
  ph --title "Absorption validation ladder" \
     --note "single / overlap / cloud: renderer vs analytic, with RMSE" \
     --out "$FIG/absorption_ladder.pdf"
fi
if [ -f "$RES/ladder/sc_single_ours.exr" ]; then
  "$PY" "$ROOT/scripts/plots/ladder_montage.py" scattering "$FIG/scattering_ladder.pdf"
else
  ph --title "Scattering validation ladder" \
     --note "renderer vs Mitsuba-analog; converged-mean difference" \
     --out "$FIG/scattering_ladder.pdf"
fi
if ls "$RES"/g1_seeds/cuda_seed*.exr >/dev/null 2>&1; then
  "$PY" "$ROOT/scripts/plots/showcase.py"
else
  ph --title "Showcase at equal quality" \
     --note "renderer vs Mitsuba-analog; firefly comparison" \
     --out "$FIG/showcase.pdf"
fi

# --- 8. Reduced-density cloud comparison (Ch 5): remedy for deep-tau display clipping ---
if ls "$RES"/reduced_density/ref_analog_seed*.exr >/dev/null 2>&1; then
  "$PY" "$ROOT/scripts/plots/reduced_density_panel.py"
else
  ph --title "Cloud at reduced density" \
     --note "ours vs Mitsuba-analog at 4% density (min T 0.17); nothing clips to black" \
     --out "$FIG/reduced_density.pdf"
fi

echo "figures regenerated into $FIG"
