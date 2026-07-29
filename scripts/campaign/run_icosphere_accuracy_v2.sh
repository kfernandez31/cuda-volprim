#!/usr/bin/env bash
# Icosphere accuracy RE-RUN (Piotr-feedback pass): reproduce tab-6.4 / fig-6.5 from scratch.
# Protocol per icosphere_port.md: absorption, white_constant, 64 spp, caps 320/496 (fits all
# three assets, 0 overflow), assets cloud/tornado/bunny, error vs the analytic arm rendered
# identically. Extends the ladder with l=4 (2562 verts) if it builds. Renders are
# deterministic (analytic escape transmittance; no scattering) -> one render per config.
# Headless: setsid nohup bash scripts/campaign/run_icosphere_accuracy_v2.sh </dev/null &
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
OUT=results/campaign/ico_v2; mkdir -p "$OUT"
grep -qxF "results/campaign/ico_v2/" .gitignore || echo "results/campaign/ico_v2/" >> .gitignore
exec > >(tee -a "$OUT/run.log") 2>&1
say(){ echo "[$(date '+%F %T')] $*"; }
say "=== icosphere accuracy v2 start ==="

restore(){ cd "$(git rev-parse --show-toplevel)"; git checkout -- device/core/constants.cuh 2>/dev/null; }
trap restore EXIT

# --- BUILD PHASE (CPU): analytic + icoN0..4, all at caps 320/496 ---
sed -i 's/#define THESIS_MAX_ACTIVE_PRIMS [0-9]*/#define THESIS_MAX_ACTIVE_PRIMS 320/;s/#define THESIS_HIT_BUFFER_CAPACITY [0-9]*/#define THESIS_HIT_BUFFER_CAPACITY 496/' device/core/constants.cuh
say "caps: $(grep -oE '#define THESIS_MAX_ACTIVE_PRIMS [0-9]+|#define THESIS_HIT_BUFFER_CAPACITY [0-9]+' device/core/constants.cuh | tr '\n' ' ')"
grep -q "#define THESIS_MAX_ACTIVE_PRIMS 320" device/core/constants.cuh || { say "CAP SED FAILED"; exit 1; }

LEVELS_OK=""
if [ ! -x build-icoAX/bin/Release/test_runner ]; then
  say "--- build analytic (320/496) ---"
  cmake -S . -B build-icoAX -DCMAKE_BUILD_TYPE=Release -DTHESIS_ICOSPHERE=OFF >/dev/null 2>&1
  cmake --build build-icoAX --target test_runner -j"$(nproc)" >/dev/null 2>&1
fi
[ -x build-icoAX/bin/Release/test_runner ] || { say "BUILD FAIL analytic"; exit 1; }
for N in 0 1 2 3 4; do
  if [ ! -x build-icoA$N/bin/Release/test_runner ]; then
    say "--- build ico N=$N (320/496) ---"
    cmake -S . -B build-icoA$N -DCMAKE_BUILD_TYPE=Release -DTHESIS_ICOSPHERE=ON -DTHESIS_ICOSPHERE_N=$N >/dev/null 2>&1
    cmake --build build-icoA$N --target test_runner -j"$(nproc)" >/dev/null 2>&1
  fi
  if [ -x build-icoA$N/bin/Release/test_runner ]; then LEVELS_OK="$LEVELS_OK $N";
  else say "BUILD FAIL N=$N (l=4 is optional; continuing)"; fi
done
restore
say "builds done; levels:$LEVELS_OK"

# --- RENDER PHASE (GPU; absorption, deterministic, fast; any power level valid) ---
declare -i OVF=0
render(){ # asset binary out
  local a=$1 bin=$2 out=$3 o=""
  [ -f "$out" ] && return 0
  if [ "$a" = cloud ]; then
    o=$(SG_ENV=white_constant SG_CAM=0 "$bin" --scene cloud_asset_validation \
        --sigma-multiplier 7.5 --spp 64 --seed 1 2>&1)
    cp test_results/cloud_asset_validation/0000.exr "$out"
  else
    o=$(SG_PLY=assets/models/$a/${a}_pyr0.ply SG_VIEW=diag SG_DIST=3.5 SG_FOV=40 SG_RES=512 \
        SG_ENV=white_constant "$bin" --scene asset_validation \
        --sigma-multiplier 10 --spp 64 --seed 1 2>&1)
    cp test_results/asset_validation/0000.exr "$out"
  fi
  grep -q "Cap overflow:" <<<"$o" && { OVF+=1; say "!!! OVERFLOW $a $(basename "$out")"; }
  say "rendered $(basename "$out")"
}
for a in cloud tornado bunny; do
  render "$a" build-icoAX/bin/Release/test_runner "$OUT/${a}_analytic.exr"
  for N in $LEVELS_OK; do
    render "$a" build-icoA$N/bin/Release/test_runner "$OUT/${a}_icoN$N.exr"
  done
done
say "renders done; overflows=$OVF"

# --- ANALYSIS PHASE (CPU): RMSE / signed mean / |D|>0.05 count vs analytic; v2 CSV ---
experiments/mitsuba-reference/.venv/bin/python - "$OUT" "$LEVELS_OK" <<'PY'
import sys, os
import numpy as np, OpenEXR, Imath
out = sys.argv[1]; levels = [int(x) for x in sys.argv[2].split()]
def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()["dataWindow"]
    w, h = dw.max.x-dw.min.x+1, dw.max.y-dw.min.y+1
    pt = Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c, pt), np.float32).reshape(h, w) for c in "RGB"], -1)
rows = ["asset,subdiv_N,rmse_vs_analytic,signed_mean,px_absdiff_gt_0.05"]
for a in ("cloud", "tornado", "bunny"):
    ref = load(f"{out}/{a}_analytic.exr")
    for N in levels:
        img = load(f"{out}/{a}_icoN{N}.exr")
        d = img - ref
        rmse = float(np.sqrt((d*d).mean())); sm = float(d.mean())
        cnt = int((np.abs(d).max(axis=-1) > 0.05).sum())
        rows.append(f"{a},{N},{rmse:.6e},{sm:+.6e},{cnt}")
        print(f"{a} N={N}: rmse={rmse:.3e} signed_mean={sm:+.3e} px>|0.05|={cnt}")
open(f"{out}/icosphere_v2.csv", "w").write("\n".join(rows) + "\n")
print("wrote", f"{out}/icosphere_v2.csv")
PY
say "=== icosphere accuracy v2 done ==="
