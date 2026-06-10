#!/usr/bin/env bash
# Regenerate every campaign figure into thesis/latex/figures/.
#
# Turnkey: drop real CSVs into results/campaign/ (schemas documented there) and rerun.
# A figure whose CSV has data becomes a real plot; otherwise it stays a watermarked
# "PROVISIONAL" placeholder. The figure floats in the thesis never change -- only the
# PDF they include. The three validation montages are assembled from renders, so they
# stay placeholders until those renders exist (see results/campaign/README.md).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="$ROOT/tools/refs/.venv/bin/python"
FIG="$ROOT/thesis/latex/figures"
RES="$ROOT/results/campaign"
mkdir -p "$FIG"

plot() { "$PY" "$ROOT/scripts/plots/figure_from_csv.py" "$@"; }
ph()   { "$PY" "$ROOT/scripts/plots/placeholder.py" "$@"; }
has_data() { [ -f "$1" ] && [ "$(grep -vcE '^[[:space:]]*#|^[[:space:]]*$' "$1")" -gt 1 ]; }

# --- 1. RR-depth efficiency (Ch 6) ---
if has_data "$RES/rr_depth.csv"; then
  plot --csv "$RES/rr_depth.csv" --x rr_depth --y frame_ms --xlabel "RR start depth" \
       --ylabel "frame time (ms)" --title "RR-depth efficiency" --out "$FIG/rr_depth.pdf"
else
  ph --title "RR-depth efficiency" --note "frame time vs RR start depth (tuned 5 -> 12)" \
     --out "$FIG/rr_depth.pdf"
fi

# --- 2. RIS K-sweep (Ch 6) ---
if has_data "$RES/ris_ksweep.csv"; then
  plot --csv "$RES/ris_ksweep.csv" --x K --y speedup_flat speedup_studio speedup_meadow \
       --xlabel "RIS candidates K" --ylabel "equal-quality speedup" --title "RIS vs MIS" \
       --out "$FIG/ris_ksweep.pdf"
else
  ph --title "RIS vs MIS, equal quality" --note "K-sweep across peakiness: flat / studio / meadow" \
     --out "$FIG/ris_ksweep.pdf"
fi

# --- 3. GAS compaction (Ch 6) ---
if has_data "$RES/gas_memory.csv"; then
  plot --csv "$RES/gas_memory.csv" --x asset --y gas_mb_uncompacted gas_mb_compacted --kind bar \
       --xlabel "asset" --ylabel "GAS size (MB)" --title "GAS compaction" --out "$FIG/gas_memory.pdf"
else
  ph --title "GAS compaction" --note "acceleration-structure size before/after, per asset" \
     --out "$FIG/gas_memory.pdf"
fi

# --- 4. Roofline (Ch 6): arithmetic intensity vs achieved performance, showing latency/memory bound ---
ph --title "Roofline" --note "arithmetic intensity vs achieved GFLOP/s (latency/memory bound)" \
   --out "$FIG/roofline.pdf"

# --- 5-7. Validation montages (Ch 5): assembled from renders, placeholder until then ---
ph --title "Absorption validation ladder" \
   --note "single / overlap / cloud: renderer vs analytic, with RMSE" \
   --out "$FIG/absorption_ladder.pdf"
ph --title "Scattering validation ladder" \
   --note "renderer vs Mitsuba-analog; converged-mean difference" \
   --out "$FIG/scattering_ladder.pdf"
ph --title "Showcase at equal quality" \
   --note "renderer vs Mitsuba-analog; firefly comparison" \
   --out "$FIG/showcase.pdf"

echo "figures regenerated into $FIG"
