#!/usr/bin/env bash
# A1 variance-source isolation. Constant-env scattering (the regime where CUDA is
# ~3x noisier than Mitsuba-analog, FINDINGS §8.15). Question: is the excess variance
# from (a) argmin OVERLAP [cloud only] or (b) the per-vertex NEE/MIS estimator
# [shows on single-G too]? Renders single-G (NO overlap) + measures noise vs Mitsuba.
set -uo pipefail
cd /home/kacper/thesis
BIN=./build/bin/Release/test_runner
PY=tools/refs/.venv/bin/python
MITS="bash tools/refs/with_jorge_mitsuba.sh tools/refs/.venv-volprim/bin/python"
SIG="${SIG:-2}"; ALB="${ALB:-0.9}"; SPP="${SPP:-512}"; SEEDS="${SEEDS:-0 1 2 3 4 5}"
OUT=renders/a1; mkdir -p "$OUT"
ts(){ date '+%H:%M:%S'; }
echo "[$(ts)] A1 isolate: single-G const-env scatter  sig=$SIG alb=$ALB spp=$SPP seeds=[$SEEDS]"
cmake --build build >/dev/null 2>&1 || { echo BUILD FAIL; exit 2; }

for s in $SEEDS; do
  o=$OUT/cuda_sg_seed$s.exr; [ -s "$o" ] && continue
  SG_ALBEDO=$ALB $BIN --scene single_gaussian_validation --sigma-multiplier $SIG \
    --spp $SPP --seed $s >/dev/null 2>&1
  cp test_results/single_gaussian_validation/0000.exr "$o"; echo "[$(ts)] $o"
done
for s in $SEEDS; do
  o=$OUT/mits_sg_seed$s.exr; [ -s "$o" ] && continue
  SG_ALBEDO=$ALB SG_SIGMA=$SIG SG_SPP=$SPP SG_SEED=$s SG_HG_G=0.85 \
    $MITS tools/refs/render_single_gaussian_via_prb.py >/dev/null 2>&1
  src=$(ls -t test_results/single_gauss/*alb${ALB}*.exr 2>/dev/null | head -1)
  if [ -n "$src" ]; then cp "$src" "$o"; echo "[$(ts)] $o"; else echo "[$(ts)] MITS MISSING"; fi
done

$PY - "$OUT" "$SIG" "$ALB" <<'PYEOF'
import sys, glob, numpy as np, OpenEXR, Imath
def load(p):
    f=OpenEXR.InputFile(p); dw=f.header()['dataWindow']
    w=dw.max.x-dw.min.x+1; h=dw.max.y-dw.min.y+1; pt=Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c,pt),np.float32).reshape(h,w) for c in('R','G','B')],-1)
OUT,SIG,ALB=sys.argv[1],sys.argv[2],sys.argv[3]
def noise(tag):
    fs=sorted(glob.glob(f'{OUT}/{tag}_sg_seed*.exr'))
    if len(fs)<2: return None
    a=np.stack([load(p) for p in fs],0)
    return len(fs), a.std(0,ddof=1).mean(), np.clip(a,0,5).std(0,ddof=1).mean(), a.mean()
print(f"\n######## A1 ISOLATION: single-G const-env scatter (sig={SIG} alb={ALB}) ########")
c=noise('cuda'); m=noise('mits')
if c and m:
    print(f"  CUDA-MIS  : seeds={c[0]} per-seed-noise={c[1]:.5f} clipped={c[2]:.5f} mean={c[3]:.4f}")
    print(f"  Mitsuba   : seeds={m[0]} per-seed-noise={m[1]:.5f} clipped={m[2]:.5f} mean={m[3]:.4f}")
    print(f"  NOISE RATIO (CUDA/Mits) = {c[1]/m[1]:.2f}x   (cloud lowsig was ~3.06x)")
    print(f"  => if ~3x here too: per-vertex estimator (NEE/MIS), NOT overlap")
    print(f"  => if ~1x here:     the 3x is argmin OVERLAP variance")
PYEOF
echo "[$(ts)] A1 isolate done"
