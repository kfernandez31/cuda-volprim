#!/usr/bin/env bash
# G6 helper: build an adaptive-sampling-ON binary (ENABLE_ADAPTIVE_SAMPLING=true, THRESHOLD=0.01) so
# we can confirm the §8.30 verdict (adaptive is a net loss at equal quality on the cloud). 150 W.
# Restores stock constants + build/. Launch headless via setsid nohup.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
exec > >(tee results/campaign/build_adaptive.log) 2>&1
echo "=== adaptive build start $(date) ==="
rm -f results/campaign/.build_adaptive.status
if git status --short device/core/constants.cuh | grep -q .; then
  echo "ABORT: constants.cuh dirty"; echo FAIL > results/campaign/.build_adaptive.status; exit 1
fi
sed -i -E 's/(ENABLE_ADAPTIVE_SAMPLING = )false/\1true/; s/(ADAPTIVE_THRESHOLD = )0\.0f/\10.01f/' device/core/constants.cuh
echo "adaptive constants now:"; grep -E "ENABLE_ADAPTIVE_SAMPLING = |ADAPTIVE_THRESHOLD = " device/core/constants.cuh
cmake --build build --target test_runner -j"$(nproc)" >/dev/null 2>&1 || {
  echo "FAIL build"; git checkout -- device/core/constants.cuh; echo FAIL > results/campaign/.build_adaptive.status; exit 1; }
cp build/bin/Release/test_runner ~/winbins/exe_adapt
cp build/device_program.optixir  ~/winbins/ir_adapt
echo "stashed exe_adapt/ir_adapt"
git checkout -- device/core/constants.cuh
cmake --build build --target test_runner -j"$(nproc)" >/dev/null 2>&1
cp build/bin/Release/test_runner ~/winbins/exe_stock 2>/dev/null || true
cp build/device_program.optixir  ~/winbins/ir_stock 2>/dev/null || true
echo -n "constants clean? "; git status --short device/core/constants.cuh | grep -q . && echo "NO" || echo "yes"
echo "=== adaptive build done $(date) ==="
echo DONE > results/campaign/.build_adaptive.status
