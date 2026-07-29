#!/usr/bin/env bash
# Remaining calibrated pairs (tornado/explosion/bunny) -> ~/winbins. 150 W, CPU-bound.
# Needed by G4 (bunny ncu), G10 (parity), G1 (bunny rung). Restores canonical stock.
# Launch:  setsid nohup bash scripts/campaign/build_calibrated_rest.sh >/dev/null 2>&1 </dev/null &
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
exec > >(tee results/campaign/build_calibrated_rest.log) 2>&1
echo "=== calibrated builds (tornado/explosion/bunny) start $(date) ==="
rm -f results/campaign/.build_rest.status
declare -A EXP=( [tornado]="112/384" [explosion]="32/160" [bunny]="80/528" )  # active/hit per cap_calibration.md

for a in tornado explosion bunny; do
  echo "--- calibrate $a ---"
  if ! scripts/tools/calibrate_caps.sh "$a"; then
    echo "FAIL: $a calibrate"; echo FAIL > results/campaign/.build_rest.status; exit 1
  fi
  A=$(grep -oP '#define THESIS_MAX_ACTIVE_PRIMS \K[0-9]+' device/core/constants.cuh)
  H=$(grep -oP '#define THESIS_HIT_BUFFER_CAPACITY \K[0-9]+' device/core/constants.cuh)
  echo "$a measured ACTIVE/HIT = $A/$H  (expected ${EXP[$a]})"
  [ "$A/$H" = "${EXP[$a]}" ] || echo "  WARN: $a caps differ from cap_calibration.md — review before trusting timings"
  cp build/bin/Release/test_runner ~/winbins/exe_$a
  cp build/device_program.optixir  ~/winbins/ir_$a
  echo "stashed exe_$a/ir_$a"
done

# restore canonical stock + rebuild build/ (rule R4)
git checkout -- device/core/constants.cuh
cmake --build build --target test_runner -j"$(nproc)" >/dev/null 2>&1
cp build/bin/Release/test_runner ~/winbins/exe_stock; cp build/device_program.optixir ~/winbins/ir_stock
echo -n "constants.cuh clean? "; git status --short device/core/constants.cuh | grep -q . && echo "NO (dirty!)" || echo "yes"

# furnace-gate each calibrated pair (correctness, power-immune)
for a in tornado explosion bunny; do
  cp ~/winbins/exe_$a build/bin/Release/test_runner; cp ~/winbins/ir_$a build/device_program.optixir
  SG_ALBEDO=1.0 build/bin/Release/test_runner --scene single_gaussian_validation --spp 1024 >/dev/null 2>&1
  echo -n "$a furnace: "; experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/furnace_check.py \
    test_results/single_gaussian_validation/0000.exr | grep -oE "=>.*"
done
cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir
echo "=== calibrated builds done $(date) ==="
echo DONE > results/campaign/.build_rest.status
