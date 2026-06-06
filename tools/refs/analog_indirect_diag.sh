#!/usr/bin/env bash
# A1 FOLLOW-UP diagnostic: does CUDA's weighted-analog indirect estimator recover
# Mitsuba-analog's depth-dropping self-averaging in conservative + constant-env media?
#
# The A1 investigation measured CUDA-NEE flat with depth vs "Mitsuba" dropping with depth,
# and concluded "fundamental NEE-vs-analog tradeoff" — but it NEVER measured a clean
# weighted-analog CUDA path (CUDA's NEE-off path is a biased unoccluded approximation, not
# analog). This adds that path (constants.cuh ANALOG_ESCAPE_ONLY) and compares, at each
# max depth, per-seed noise for THREE estimators on single-G const-env high-albedo:
#   (1) CUDA-NEE      — production (ANALOG_ESCAPE_ONLY=false)
#   (2) CUDA-analog   — new weighted-analog (ANALOG_ESCAPE_ONLY=true)
#   (3) Mitsuba-analog — volprim_prb with SG_NEE=0
#
# HYPOTHESIS: CUDA-analog drops with depth like Mitsuba-analog (means match = unbiased),
# confirming the self-averaging is recoverable → motivates the MIS-combined implementation.
# Restores constants.cuh on exit.
set -uo pipefail
cd /home/kacper/thesis
BIN=./build/bin/Release/test_runner
PY=tools/refs/.venv/bin/python
MITS="bash tools/refs/with_jorge_mitsuba.sh tools/refs/.venv-volprim/bin/python"
SIG="${SIG:-2}"; ALB="${ALB:-0.9}"; SPP="${SPP:-512}"; SEEDS="${SEEDS:-0 1 2 3 4 5}"
DEPTHS="${DEPTHS:-1 2 4 8}"
OUT=renders/analog_diag; mkdir -p "$OUT"
ts(){ date '+%H:%M:%S'; }
cleanup(){ git checkout -- device/core/constants.cuh 2>/dev/null; cmake --build build >/dev/null 2>&1; }
trap cleanup EXIT

set_depth(){  # $1 = depth
  git checkout -- device/core/constants.cuh
  sed -i "s/^    128;  \/\/ Mitsuba production: 64-128/    $1;  \/\/ sweep/" device/core/constants.cuh
}
set_analog(){  # $1 = true|false  (must be called AFTER set_depth, before build)
  sed -i "s/^constexpr bool ANALOG_ESCAPE_ONLY = .*/constexpr bool ANALOG_ESCAPE_ONLY = $1;/" device/core/constants.cuh
}

echo "[$(ts)] analog-indirect diagnostic  sig=$SIG alb=$ALB spp=$SPP depths=[$DEPTHS] seeds=[$SEEDS]"

for D in $DEPTHS; do
  # (1) CUDA-NEE
  set_depth $D; set_analog false
  cmake --build build >/dev/null 2>&1 || { echo "BUILD FAIL nee D=$D"; exit 2; }
  for s in $SEEDS; do
    SG_ALBEDO=$ALB $BIN --scene single_gaussian_validation --sigma-multiplier $SIG \
      --spp $SPP --seed $s >/dev/null 2>&1
    cp test_results/single_gaussian_validation/0000.exr "$OUT/cudaNEE_d${D}_seed$s.exr"
  done
  echo "[$(ts)] depth $D: CUDA-NEE done"

  # (2) CUDA-analog
  set_depth $D; set_analog true
  cmake --build build >/dev/null 2>&1 || { echo "BUILD FAIL analog D=$D"; exit 2; }
  for s in $SEEDS; do
    SG_ALBEDO=$ALB $BIN --scene single_gaussian_validation --sigma-multiplier $SIG \
      --spp $SPP --seed $s >/dev/null 2>&1
    cp test_results/single_gaussian_validation/0000.exr "$OUT/cudaANA_d${D}_seed$s.exr"
  done
  echo "[$(ts)] depth $D: CUDA-analog done"

  # (3) Mitsuba-analog (NEE off)
  for s in $SEEDS; do
    SG_ALBEDO=$ALB SG_SIGMA=$SIG SG_SPP=$SPP SG_SEED=$s SG_HG_G=0.85 SG_MAX_DEPTH=$D SG_NEE=0 \
      $MITS tools/refs/render_single_gaussian_via_prb.py >/dev/null 2>&1
    src=$(ls -t test_results/single_gauss/*alb${ALB}*.exr 2>/dev/null | head -1)
    [ -n "$src" ] && cp "$src" "$OUT/mitsANA_d${D}_seed$s.exr"
  done
  echo "[$(ts)] depth $D: Mitsuba-analog done"
done

$PY - "$OUT" "$DEPTHS" <<'PYEOF'
import sys, glob, numpy as np, OpenEXR, Imath
def load(p):
    f=OpenEXR.InputFile(p); dw=f.header()['dataWindow']
    w=dw.max.x-dw.min.x+1; h=dw.max.y-dw.min.y+1; pt=Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c,pt),np.float32).reshape(h,w) for c in('R','G','B')],-1)
OUT=sys.argv[1]; DEPTHS=[int(x) for x in sys.argv[2].split()]
def stat(tag):
    fs=sorted(glob.glob(f'{OUT}/{tag}_seed*.exr'))
    if len(fs)<2: return None
    a=np.stack([load(p) for p in fs],0); return a.std(0,ddof=1).mean(), a.mean()
print("\n############ A1 FOLLOW-UP: weighted-analog indirect (single-G const-env) ############")
print("       CUDA-NEE          CUDA-analog        Mitsuba-analog")
print("depth  noise    mean     noise    mean      noise    mean   | ana/mits  nee/mits")
for D in DEPTHS:
    n=stat(f'cudaNEE_d{D}'); a=stat(f'cudaANA_d{D}'); m=stat(f'mitsANA_d{D}')
    if n and a and m:
        ar = a[0]/m[0] if m[0] else float('nan')
        nr = n[0]/m[0] if m[0] else float('nan')
        print(f"  {D:2d}   {n[0]:.5f} {n[1]:.4f}   {a[0]:.5f} {a[1]:.4f}    {m[0]:.5f} {m[1]:.4f} | {ar:6.2f}x  {nr:6.2f}x")
print("\nREAD: CUDA-analog noise should DROP with depth & track Mitsuba-analog (ana/mits→~1).")
print("      Means must agree across all three (unbiased). If CUDA-analog self-averages,")
print("      the MIS-combined estimator is worth implementing.")
PYEOF
echo "[$(ts)] analog-indirect diagnostic done"
