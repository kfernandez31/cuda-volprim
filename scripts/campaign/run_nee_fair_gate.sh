#!/usr/bin/env bash
# Gate G1 — pixelwise cross-renderer GT agreement (Task 0, fair-NEE campaign). 150 W is sufficient.
#
# Renders the corrected-NEE ground truth (volprim NEE with our 4-fix correction) and the patched-analog
# G2 cross-check, then compares against our banked ours-MIS GT (results/campaign/g1_seeds/cuda_seed*.exr).
# The fork with all four fixes is at results/campaign/nee_fix/GaborFixed (do NOT recreate it).
set -euo pipefail
cd "$(dirname "$0")/../.."

F=results/campaign/nee_fix/GaborFixed
CLOUD_DIR=$PWD/assets/models/cloud
MEADOW_HDR=$PWD/assets/environment_maps/meadow_2_4k.hdr
OUT=results/campaign/nee_fair
mkdir -p "$OUT/gt"

common_env() {
  VOLPRIM_DIR=$PWD/$F CLOUD_DIR=$CLOUD_DIR MEADOW_HDR=$MEADOW_HDR SG_ENV=meadow
}

# --- corrected-NEE GT: 2 seeds x 2048 spp (~50-60 min/seed at 150 W). Idempotent: skips existing EXRs. ---
for seed in 0 1; do
  if [ -f "$OUT/gt/gabor_nee_meadow_spp2048_seed${seed}.exr" ]; then
    echo "=== corrected-NEE GT seed=$seed already present, skipping ==="; continue
  fi
  echo "=== corrected-NEE GT seed=$seed (2048 spp) ==="
  VOLPRIM_DIR=$PWD/$F CLOUD_DIR=$CLOUD_DIR MEADOW_HDR=$MEADOW_HDR \
    SG_ENV=meadow SG_NEE=1 SG_SPP=2048 SG_SEED=$seed SG_OUTDIR=$OUT/gt \
    experiments/mitsuba-reference/with_pip_gabor.sh experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/gabor_cloud.py 2>&1 | grep -E "RESULT|Error|Traceback"
done

# --- PRIMARY gate: pixelwise comparison ours-MIS vs corrected-NEE (needs only the 2 GT seeds). Run it
#     FIRST so the kill-switch verdict lands ASAP (~1 h), before the slower G2 analog renders. ---
echo "=== PRIMARY GATE compare (ours-MIS vs corrected-NEE) ==="
experiments/mitsuba-reference/with_pip_gabor.sh experiments/mitsuba-reference/.venv/bin/python scripts/plots/nee_fair_gate.py 2>&1 | tee "$OUT/gate_verdict.log"

# --- G2 patched-analog cross-check: 4 seeds x 1024 spp (is seed-avg analog within CI of NEE 0.3225?) ---
for seed in 0 1 2 3; do
  if [ -f "$OUT/g2_analog/gabor_analog_meadow_spp1024_seed${seed}.exr" ]; then
    echo "=== patched-analog G2 seed=$seed already present, skipping ==="; continue
  fi
  echo "=== patched-analog G2 seed=$seed (1024 spp) ==="
  VOLPRIM_DIR=$PWD/$F CLOUD_DIR=$CLOUD_DIR MEADOW_HDR=$MEADOW_HDR \
    SG_ENV=meadow SG_NEE=0 SG_SPP=1024 SG_SEED=$seed SG_OUTDIR=$OUT/g2_analog \
    experiments/mitsuba-reference/with_pip_gabor.sh experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/gabor_cloud.py 2>&1 | grep -E "RESULT|Error|Traceback"
done
echo "=== G2 analog renders done; G2 numeric check runs separately ==="
