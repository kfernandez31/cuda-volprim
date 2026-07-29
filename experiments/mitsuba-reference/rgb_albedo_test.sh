#!/usr/bin/env bash
# Colored (per-channel) RGB albedo validation: a TINTED single Gaussian (albedo
# R>G>B) vs Mitsuba-analog. Confirms wavelength-dependent scattering matches and
# that the medium tints correctly (the path has no cross-channel coupling, so this
# just exercises the float3 albedo read + per-channel throughput). Idempotent.
set -uo pipefail
cd /home/kacper/thesis
BIN=./build/bin/Release/test_runner
PY=experiments/mitsuba-reference/.venv/bin/python
MITS="bash experiments/mitsuba-reference/with_jorge_mitsuba.sh experiments/mitsuba-reference/.venv-volprim/bin/python"
ALB="0.9,0.5,0.2"; SIG=4; SPP=2048
OUT=renders/rgb_albedo; RES=renders/rgb_albedo_results.txt
mkdir -p "$OUT"
ts(){ date '+%H:%M:%S'; }
echo "[$(ts)] RGB albedo test: albedo=$ALB sigma=$SIG spp=$SPP"
cmake --build build >/dev/null 2>&1 || { echo "BUILD FAIL"; exit 2; }

# CUDA (tinted single Gaussian, constant white env, scattering)
for s in 0 1 2; do
  o=$OUT/cuda_seed$s.exr; [ -s "$o" ] && continue
  SG_ALBEDO=$ALB $BIN --scene single_gaussian_validation --sigma-multiplier $SIG \
    --spp $SPP --seed $s >/dev/null 2>&1
  cp test_results/single_gaussian_validation/0000.exr "$o"; echo "[$(ts)] wrote $o"
done

# Mitsuba-analog, matched. Collect the newest matching EXR (robust to filename tags).
for s in 0 1 2; do
  o=$OUT/mits_seed$s.exr; [ -s "$o" ] && continue
  SG_ALBEDO=$ALB SG_SIGMA=$SIG SG_SPP=$SPP SG_SEED=$s \
    $MITS experiments/mitsuba-reference/render_single_gaussian_via_prb.py >/dev/null 2>&1
  src=$(ls -t test_results/single_gauss/*alb0.90-0.50-0.20*.exr 2>/dev/null | head -1)
  if [ -n "$src" ] && [ -s "$src" ]; then cp "$src" "$o"; echo "[$(ts)] wrote $o";
  else echo "[$(ts)] MITS MISSING (alb tag)"; fi
done

{
  echo "######## RGB ALBEDO (tinted single-G $ALB) — CUDA vs Mitsuba-analog ########"
  $PY experiments/mitsuba-reference/sg_systematic.py "$OUT/cuda_seed*.exr" "$OUT/mits_seed*.exr" 2>/dev/null \
    | grep -E "global:|^  R:|^  G:|^  B:|median"
  echo "--- per-channel MEANS (tint sanity: expect R > G > B on BOTH) ---"
  $PY - "$OUT" <<'PYEOF'
import sys, glob, numpy as np, OpenEXR, Imath
def load(p):
    f=OpenEXR.InputFile(p); dw=f.header()['dataWindow']
    w=dw.max.x-dw.min.x+1; h=dw.max.y-dw.min.y+1; pt=Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c,pt),np.float32).reshape(h,w) for c in('R','G','B')],-1)
OUT=sys.argv[1]
for tag in ('cuda','mits'):
    fs=glob.glob(f'{OUT}/{tag}_seed*.exr')
    if not fs: print(f'{tag}: (no renders)'); continue
    a=np.mean([load(p) for p in fs],0)
    print(f'{tag}: R={a[...,0].mean():.4f} G={a[...,1].mean():.4f} B={a[...,2].mean():.4f}')
PYEOF
} >> "$RES"
echo "[$(ts)] RGB ALBEDO DONE -> $RES"; cat "$RES"
