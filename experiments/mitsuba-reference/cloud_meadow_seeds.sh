#!/usr/bin/env bash
# WS1/WS4: multi-seed cloud-under-meadow systematic (cam 0). Idempotent/resumable:
# skips seeds whose EXR already exists. Renders CUDA seeds then Mitsuba-analog seeds.
# DO NOT rebuild build/ while this runs (test_runner reloads the optixir per seed).
#
# Env: OUT (output dir), SG_HG_G (Mitsuba HG g; CUDA gets HG from the build).
# Usage: OUT=renders/cloud_meadow_hg SG_HG_G=0.85 \
#          setsid nohup bash experiments/mitsuba-reference/cloud_meadow_seeds.sh <NSEEDS> <SPP> > log 2>&1 < /dev/null &
set -euo pipefail
cd /home/kacper/thesis
NSEEDS="${1:-8}"
SPP="${2:-256}"
OUT="${OUT:-renders/cloud_meadow}"
HG="${SG_HG_G:-}"
mkdir -p "$OUT"

# Mitsuba cloud output dir mirrors render_cloud_prb_absorption.py: refs_prb_scattering_meadow[_hgG]
MITS_DIR="assets/models/cloud/refs_prb_scattering_meadow"
if [ -n "$HG" ] && [ "$HG" != "0" ] && [ "$HG" != "0.0" ]; then
  MITS_DIR="${MITS_DIR}_hg$(LC_ALL=C printf %.2f "$HG")"
fi

echo "=== CUDA cloud-meadow, $NSEEDS seeds @ $SPP spp  (OUT=$OUT, HG from build) ==="
for S in $(seq 0 $((NSEEDS-1))); do
  f="$OUT/cuda_seed$(printf %02d "$S").exr"
  if [ -f "$f" ]; then echo "skip $f"; continue; fi
  SG_ENV=meadow SG_CAM=0 ./build/bin/Release/test_runner \
    --scene cloud_asset_scattering --sigma-multiplier 7.5 --spp "$SPP" --seed "$S" >/dev/null 2>&1
  cp test_results/cloud_asset_scattering/0000.exr "$f"
  echo "wrote $f"
done

echo "=== Mitsuba cloud-meadow analog, $NSEEDS seeds @ $SPP spp  (SG_HG_G='$HG', dir=$MITS_DIR) ==="
for S in $(seq 0 $((NSEEDS-1))); do
  f="$OUT/mits_seed$(printf %02d "$S").exr"
  if [ -f "$f" ]; then echo "skip $f"; continue; fi
  SG_ENV=meadow SG_CAM=0 SG_ALBEDO=0.9 SG_SIGMA=7.5 SG_SPP="$SPP" SG_SEED="$S" SG_HG_G="$HG" \
    experiments/mitsuba-reference/with_jorge_mitsuba.sh experiments/mitsuba-reference/.venv/bin/python \
    experiments/mitsuba-reference/render_cloud_prb_absorption.py >/dev/null 2>&1
  cp "$MITS_DIR/0000.exr" "$f"
  echo "wrote $f"
done
echo "ALL DONE"
