#!/usr/bin/env bash
# One-command per-workload cap calibration (spec: 2026-06-12-cap-calibration-design.md).
# Measures true cap demand with --measure-caps on the asset's scattering stress
# (the binding workload, per caps_per_asset.md), writes the two constants, rebuilds,
# and verifies the calibrated build renders the stress with zero overflows.
# Usage: scripts/tools/calibrate_caps.sh <cloud|tornado|explosion|bunny> [spp] [seed...]
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

ASSET="${1:?usage: calibrate_caps.sh <asset> [spp] [seeds...]}"
SPP="${2:-16}"
if [ $# -ge 3 ]; then shift 2; SEEDS=("$@"); else SEEDS=(42 43); fi
BIN=build/bin/Release/test_runner
CONSTS=device/core/constants.cuh

run_asset() {  # $1 = seed, $2 = extra flags → echoes full output
  # NOTE: $2 is deliberately UNQUOTED — when empty it must expand to zero words (quoting it would pass an empty positional to the binary).
  case "$ASSET" in
    cloud)
      SG_ENV=meadow SG_CAM=0 $BIN --scene cloud_asset_scattering \
        --sigma-multiplier 7.5 --spp "$SPP" --seed "$1" $2 2>&1 ;;
    tornado|explosion|bunny)
      SG_PLY=assets/models/$ASSET/${ASSET}_pyr0.ply SG_ENV=meadow SG_ALBEDO=0.9 \
        SG_RES=512 SG_VIEW=diag $BIN --scene asset_validation \
        --spp "$SPP" --seed "$1" $2 2>&1 ;;
    *) echo "unknown asset: $ASSET" >&2; exit 2 ;;
  esac
}

H_MAX=0; A_MAX=0
for S in "${SEEDS[@]}"; do
  OUT=$(run_asset "$S" --measure-caps)
  if ! read -r H A < <(echo "$OUT" | sed -n 's/.*max hits\/ray = \([0-9]*\), max point-overlap = \([0-9]*\).*/\1 \2/p'); then
    echo "FAIL: no measurement line (seed $S)"; exit 1
  fi
  echo "  seed $S: hits/ray=$H overlap=$A"
  (( H > H_MAX )) && H_MAX=$H
  (( A > A_MAX )) && A_MAX=$A
done

suggest() { echo $(( ( ( ($1 * 9 + 7) / 8 + 15 ) / 16 ) * 16 )); }
H_CAP=$(suggest "$H_MAX"); A_CAP=$(suggest "$A_MAX")
echo "Measured maxima: hits/ray=$H_MAX overlap=$A_MAX -> caps HIT=$H_CAP ACTIVE=$A_CAP"

restore() { git checkout -- "$CONSTS"; }
trap restore ERR
sed -i "s/#define THESIS_MAX_ACTIVE_PRIMS [0-9]*/#define THESIS_MAX_ACTIVE_PRIMS $A_CAP/;s/#define THESIS_HIT_BUFFER_CAPACITY [0-9]*/#define THESIS_HIT_BUFFER_CAPACITY $H_CAP/" "$CONSTS"
cmake --build build -j"$(nproc)" >/dev/null

VERIFY_SEED=7   # deliberately UNMEASURED: validates the 1.125 margin
VOUT=$(run_asset $VERIFY_SEED "")
if grep -q "Cap overflow:" <<<"$VOUT"; then
  echo "VERIFY FAIL: calibrated caps overflowed on unmeasured seed $VERIFY_SEED"
  restore
  echo "NOTE: build/ still holds the failed caps — rebuild after this restore."
  exit 1
fi
echo "VERIFY OK: $ASSET caps HIT_BUFFER_CAPACITY=$H_CAP MAX_ACTIVE_PRIMS=$A_CAP (no overflow, seed $VERIFY_SEED)"
echo "NOTE: $CONSTS now holds the calibrated caps (not committed)."
