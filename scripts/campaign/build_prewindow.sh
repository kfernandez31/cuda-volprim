#!/usr/bin/env bash
# Pre-window builds (150 W, CPU-bound): the cloud calibrated pair + fast-erf pair,
# the only binaries the 11:30-13:30 timing window consumes. Restores canonical stock.
# Launch headless:  setsid nohup bash scripts/campaign/build_prewindow.sh >/dev/null 2>&1 </dev/null &
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
exec > >(tee results/campaign/build_prewindow.log) 2>&1
echo "=== pre-window builds start $(date) ==="
mkdir -p ~/winbins
rm -f results/campaign/.build_prewindow.status

# 1. cloud calibrated pair (calibrate_caps leaves constants at the measured caps on success)
echo "--- [1] cloud calibrate ---"
if ! scripts/tools/calibrate_caps.sh cloud; then
  echo "FAIL: cloud calibrate"; echo FAIL > results/campaign/.build_prewindow.status; exit 1
fi
echo "constants now:"; grep -E "#define THESIS_(MAX_ACTIVE_PRIMS|HIT_BUFFER_CAPACITY) " device/core/constants.cuh
cp build/bin/Release/test_runner ~/winbins/exe_cloud
cp build/device_program.optixir  ~/winbins/ir_cloud
echo "stashed exe_cloud/ir_cloud"

# 2. fast-erf pair at the SAME caps (constants still calibrated here). build-ferf kept ALIVE
#    (rule R3: the exe bakes build-ferf's absolute optixir path -> run it from build-ferf/, do not relocate).
echo "--- [2] fast-erf build (build-ferf, same caps) ---"
cmake -S . -B build-ferf -DCMAKE_BUILD_TYPE=Release -DTHESIS_ENABLE_FAST_ERF=ON >/dev/null 2>&1
cmake --build build-ferf --target test_runner -j"$(nproc)" >/dev/null 2>&1
if [ -x build-ferf/bin/Release/test_runner ] && [ -f build-ferf/device_program.optixir ]; then
  echo "fast-erf OK: $(ls -la build-ferf/bin/Release/test_runner | awk '{print $5}') bytes (run from build-ferf/ per R3)"
else
  echo "WARN: fast-erf build incomplete — window will skip the fast-erf A/B"
fi

# 3. restore canonical stock constants + rebuild build/ (rule R4)
echo "--- [3] restore stock + rebuild build/ ---"
git checkout -- device/core/constants.cuh
cmake --build build --target test_runner -j"$(nproc)" >/dev/null 2>&1
cp build/bin/Release/test_runner ~/winbins/exe_stock
cp build/device_program.optixir  ~/winbins/ir_stock
echo -n "constants.cuh clean? "; git status --short device/core/constants.cuh | grep -q . && echo "NO (dirty!)" || echo "yes"

# 4. furnace-gate the cloud calibrated pair (correctness, power-immune)
echo "--- [4] furnace gate cloud calibrated ---"
cp ~/winbins/exe_cloud build/bin/Release/test_runner; cp ~/winbins/ir_cloud build/device_program.optixir
SG_ALBEDO=1.0 build/bin/Release/test_runner --scene single_gaussian_validation --spp 1024 >/dev/null 2>&1
echo -n "cloud furnace: "; experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/furnace_check.py \
  test_results/single_gaussian_validation/0000.exr | grep -oE "=>.*"
# leave build/ holding the canonical stock pair
cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir

echo "=== pre-window builds done $(date) ==="
echo DONE > results/campaign/.build_prewindow.status
