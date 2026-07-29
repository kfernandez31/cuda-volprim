#!/usr/bin/env bash
# A1 diagnostic: where does CUDA's flat-env excess variance enter? Sweep max path
# depth; measure CUDA-vs-Mitsuba per-seed noise at each. Gap at depth 1 => direct
# term; gap grows with depth => multiple-scatter chain. Restores constants.cuh on exit.
set -uo pipefail
cd /home/kacper/thesis
BIN=./build/bin/Release/test_runner
PY=experiments/mitsuba-reference/.venv/bin/python
MITS="bash experiments/mitsuba-reference/with_jorge_mitsuba.sh experiments/mitsuba-reference/.venv-volprim/bin/python"
SIG=2; ALB=0.9; SPP=512; SEEDS="0 1 2 3 4 5"
OUT=renders/a1/depth; mkdir -p "$OUT"
ts(){ date '+%H:%M:%S'; }
cleanup(){ git checkout -- device/core/constants.cuh 2>/dev/null; cmake --build build >/dev/null 2>&1; }
trap cleanup EXIT

for D in 1 2 4 8; do
  git checkout -- device/core/constants.cuh
  sed -i "s/^    128;  \/\/ Mitsuba production: 64-128/    $D;  \/\/ sweep/" device/core/constants.cuh
  cmake --build build >/dev/null 2>&1 || { echo "BUILD FAIL D=$D"; exit 2; }
  echo "[$(ts)] depth $D: rendering CUDA + Mitsuba"
  for s in $SEEDS; do
    SG_ALBEDO=$ALB $BIN --scene single_gaussian_validation --sigma-multiplier $SIG \
      --spp $SPP --seed $s >/dev/null 2>&1
    cp test_results/single_gaussian_validation/0000.exr "$OUT/cuda_d${D}_seed$s.exr"
  done
  for s in $SEEDS; do
    SG_ALBEDO=$ALB SG_SIGMA=$SIG SG_SPP=$SPP SG_SEED=$s SG_HG_G=0.85 SG_MAX_DEPTH=$D \
      $MITS experiments/mitsuba-reference/render_single_gaussian_via_prb.py >/dev/null 2>&1
    src=$(ls -t test_results/single_gauss/*alb${ALB}*.exr 2>/dev/null | head -1)
    [ -n "$src" ] && cp "$src" "$OUT/mits_d${D}_seed$s.exr"
  done
done

$PY - "$OUT" <<'PYEOF'
import sys, glob, numpy as np, OpenEXR, Imath
def load(p):
    f=OpenEXR.InputFile(p); dw=f.header()['dataWindow']
    w=dw.max.x-dw.min.x+1; h=dw.max.y-dw.min.y+1; pt=Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c,pt),np.float32).reshape(h,w) for c in('R','G','B')],-1)
OUT=sys.argv[1]
def noise(tag):
    fs=sorted(glob.glob(f'{OUT}/{tag}_seed*.exr'))
    if len(fs)<2: return None
    a=np.stack([load(p) for p in fs],0); return a.std(0,ddof=1).mean(), a.mean()
print("\n######## A1 DEPTH SWEEP: CUDA vs Mitsuba noise by max depth (single-G const-env) ########")
print("depth  CUDA-noise  Mits-noise  ratio   CUDA-mean  Mits-mean")
for D in (1,2,4,8):
    c=noise(f'cuda_d{D}'); m=noise(f'mits_d{D}')
    if c and m:
        print(f"  {D:2d}   {c[0]:.5f}     {m[0]:.5f}    {c[0]/m[0]:5.2f}x   {c[1]:.4f}    {m[1]:.4f}")
PYEOF
echo "[$(ts)] depth sweep done"
