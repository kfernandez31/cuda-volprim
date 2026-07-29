#!/usr/bin/env bash
# G1 comparison A: build a TRUE pure-analog binary (ENABLE_NEE=false + ENABLE_ANALYTIC_DIRECT=false,
# with the raygen unoccluded-single-scatter term removed) at the cloud's calibrated caps (64/96).
# Validates furnace (analog must conserve energy) + cloud-meadow analog mean (~0.32 = Mitsuba-analog).
# Restores stock. Launch: setsid nohup bash scripts/campaign/build_analog.sh >/dev/null 2>&1 </dev/null &
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
exec > >(tee results/campaign/build_analog.log) 2>&1
echo "=== analog build start $(date) ==="
rm -f results/campaign/.build_analog.status
if git status --short device/core/constants.cuh | grep -q .; then
  echo "ABORT: constants dirty"; echo FAIL > results/campaign/.build_analog.status; exit 1; fi
sed -i -E 's/(ENABLE_NEE = )true/\1false/; s/(ENABLE_ANALYTIC_DIRECT = )true/\1false/; s/(MAX_ACTIVE_PRIMS = )[0-9]+/\164/; s/(HIT_BUFFER_CAPACITY = )[0-9]+/\196/' device/core/constants.cuh
echo "constants now:"; grep -E "ENABLE_NEE = |ENABLE_ANALYTIC_DIRECT = |MAX_ACTIVE_PRIMS = |HIT_BUFFER_CAPACITY = " device/core/constants.cuh
cmake --build build --target test_runner -j"$(nproc)" >/dev/null 2>&1 || {
  echo "BUILD FAIL"; git checkout -- device/core/constants.cuh; echo FAIL > results/campaign/.build_analog.status; exit 1; }
cp build/bin/Release/test_runner ~/winbins/exe_analog; cp build/device_program.optixir ~/winbins/ir_analog
echo "stashed exe_analog/ir_analog"

# --- validate (still on the analog binary in build/) ---
echo "--- furnace (analog must be flat ~1.0) ---"
SG_ALBEDO=1.0 build/bin/Release/test_runner --scene single_gaussian_validation --spp 1024 >/dev/null 2>&1
experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/furnace_check.py test_results/single_gaussian_validation/0000.exr | grep -oE "=>.*" || echo "(furnace check parse)"
echo "--- cloud-meadow analog mean (expect ~0.32 = Mitsuba-analog 0.3201, NOT 5.5) ---"
SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering --sigma-multiplier 7.5 --spp 64 --seed 0 2>&1 | grep -iE "overflow|total time"
experiments/mitsuba-reference/.venv/bin/python - <<'PY'
import OpenEXR, Imath, numpy as np
f=OpenEXR.InputFile('test_results/cloud_asset_scattering/0000.exr'); dw=f.header()['dataWindow']
w=dw.max.x-dw.min.x+1; h=dw.max.y-dw.min.y+1; pt=Imath.PixelType(Imath.PixelType.FLOAT)
im=np.stack([np.frombuffer(f.channel(c,pt),np.float32).reshape(h,w) for c in('R','G','B')],-1)
print(f'analog cloud-meadow mean = {im.mean():.4f}  (target ~0.32; >5 = still broken)')
PY

# restore stock
git checkout -- device/core/constants.cuh
cmake --build build --target test_runner -j"$(nproc)" >/dev/null 2>&1
cp build/bin/Release/test_runner ~/winbins/exe_stock 2>/dev/null || true
cp build/device_program.optixir ~/winbins/ir_stock 2>/dev/null || true
echo -n "constants clean? "; git status --short device/core/constants.cuh | grep -q . && echo "NO" || echo "yes"
echo "=== analog build done $(date) ==="
echo DONE > results/campaign/.build_analog.status
