#!/usr/bin/env bash
# G5b helper: build a SAFE 512/512-cap binary (the conservative pre-calibration sizing) so we can
# measure the VRAM that per-asset cap calibration actually saves. 150 W / CPU-bound. Restores stock.
# Launch: setsid nohup bash scripts/campaign/build_safe512.sh >/dev/null 2>&1 </dev/null &
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
exec > >(tee results/campaign/build_safe512.log) 2>&1
echo "=== SAFE-512 build start $(date) ==="
rm -f results/campaign/.build_safe512.status
mkdir -p ~/winbins

# guard: constants must be clean stock before we edit
if git status --short device/core/constants.cuh | grep -q .; then
  echo "ABORT: constants.cuh dirty before edit"; echo FAIL > results/campaign/.build_safe512.status; exit 1
fi

sed -i -E 's/(MAX_ACTIVE_PRIMS = )[0-9]+/\1512/; s/(HIT_BUFFER_CAPACITY = )[0-9]+/\1512/' device/core/constants.cuh
echo "caps now:"; grep -E "MAX_ACTIVE_PRIMS = |HIT_BUFFER_CAPACITY = " device/core/constants.cuh

cmake --build build --target test_runner -j"$(nproc)" >/dev/null 2>&1 || {
  echo "FAIL: 512 build"; git checkout -- device/core/constants.cuh; echo FAIL > results/campaign/.build_safe512.status; exit 1; }
cp build/bin/Release/test_runner ~/winbins/exe_safe512
cp build/device_program.optixir  ~/winbins/ir_safe512
echo "stashed exe_safe512/ir_safe512"

# restore stock + rebuild build/ (rule R4)
git checkout -- device/core/constants.cuh
cmake --build build --target test_runner -j"$(nproc)" >/dev/null 2>&1
cp build/bin/Release/test_runner ~/winbins/exe_stock 2>/dev/null || true
cp build/device_program.optixir  ~/winbins/ir_stock 2>/dev/null || true
echo -n "constants.cuh clean? "; git status --short device/core/constants.cuh | grep -q . && echo "NO (dirty!)" || echo "yes"
echo "=== SAFE-512 build done $(date) ==="
echo DONE > results/campaign/.build_safe512.status
