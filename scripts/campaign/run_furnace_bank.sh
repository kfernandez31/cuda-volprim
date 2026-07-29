#!/usr/bin/env bash
# Bank the furnace energy-conservation gate (B1 evidence, reference-free, clock-independent).
# Albedo=1, white-constant env (value 1), single Gaussian: a correct estimator returns the
# background (mean=1) for ANY phase/MIS setting. Banks: ours, Mitsuba-analog, Mitsuba-NEE at
# three optical depths (sigma 2/6/12) to measure how the NEE over-count grows with thickness.
set -uo pipefail
cd /home/kacper/thesis
OUT=results/campaign/furnace; mkdir -p "$OUT"
LOG=results/campaign/furnace.md
PY=experiments/mitsuba-reference/.venv/bin/python
BIN=./build/bin/Release/test_runner
SPP=1024
M=experiments/mitsuba-reference/with_jorge_mitsuba.sh

bias(){ $PY experiments/mitsuba-reference/furnace_check.py "$1" 1.0 2>&1 | grep -E "mean=|=>"; }

{
echo "# Furnace energy-conservation gate (B1 evidence) — $(date -u)"
echo
echo "Reference-free: albedo=1, white-constant env (value 1), single Gaussian, ${SPP} spp."
echo "A correct estimator returns the background (mean=1) for any phase/MIS setting. Mitsuba-NEE swept"
echo "over sigma to show the over-count grows with optical thickness (the furnace->cloud magnitude bridge)."
echo '```'
} > "$LOG"

echo "=== OURS (MIS+NEE path, albedo=1, sigma=6) ===" | tee -a "$LOG"
SG_ALBEDO=1.0 $BIN --scene single_gaussian_validation --sigma-multiplier 6 --spp $SPP >/dev/null 2>&1
cp test_results/single_gaussian_validation/0000.exr "$OUT/ours_furnace_s6.exr"
bias "$OUT/ours_furnace_s6.exr" | tee -a "$LOG"

echo "=== MITSUBA analog (use_nee=0, sigma=6) ===" | tee -a "$LOG"
SG_ALBEDO=1.0 SG_ENV=white_constant SG_NEE=0 SG_SIGMA=6 SG_SPP=$SPP \
  $M $PY experiments/mitsuba-reference/render_single_gaussian_via_prb.py 2>&1 | grep -E "wrote|mean" | tee -a "$LOG"
cp "$(ls -t test_results/single_gauss/*.exr | head -1)" "$OUT/mits_analog_furnace_s6.exr"
bias "$OUT/mits_analog_furnace_s6.exr" | tee -a "$LOG"

for S in 2 6 12; do
  echo "=== MITSUBA NEE (use_nee=1, sigma=$S) ===" | tee -a "$LOG"
  SG_ALBEDO=1.0 SG_ENV=white_constant SG_NEE=1 SG_SIGMA=$S SG_SPP=$SPP \
    $M $PY experiments/mitsuba-reference/render_single_gaussian_via_prb.py 2>&1 | grep -E "wrote|mean" | tee -a "$LOG"
  cp "$(ls -t test_results/single_gauss/*.exr | head -1)" "$OUT/mits_nee_furnace_s${S}.exr"
  bias "$OUT/mits_nee_furnace_s${S}.exr" | tee -a "$LOG"
done

echo '```' >> "$LOG"
echo "=== FURNACE BANK DONE $(date -u) ==="
