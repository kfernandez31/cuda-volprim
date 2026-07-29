#!/usr/bin/env bash
# S4 discriminating test: is the +2e-4 heavy-overlap cluster residual due to single-precision erf^-1?
# Render the overlapping cluster with the shipped float32 erfinvf (build/) and a double-precision
# erfinv variant (build-hiprec, THESIS_HIPREC_ERFINV) at identical seed/spp/sigma/albedo, then diff.
# If they differ at the overlap by ~2e-4 -> erf^-1 precision is the cause; if << -> it is not (the
# residual is then the independence/containment effect, the other candidate). Clock-independent.
set -uo pipefail
cd /home/kacper/thesis
OUT=results/campaign/s4_erfinv; mkdir -p "$OUT"
PY=experiments/mitsuba-reference/.venv/bin/python
SCENE=cluster_validation; SPP=16384; SIG=2; ALB=0.9
for S in 1 2; do
  SG_ALBEDO=$ALB build/bin/Release/test_runner --scene $SCENE --sigma-multiplier $SIG --spp $SPP --seed $S >/dev/null 2>&1
  cp test_results/$SCENE/0000.exr "$OUT/f32_s${S}.exr"
  SG_ALBEDO=$ALB build-hiprec/bin/Release/test_runner --scene $SCENE --sigma-multiplier $SIG --spp $SPP --seed $S >/dev/null 2>&1
  cp test_results/$SCENE/0000.exr "$OUT/hiprec_s${S}.exr"
done
$PY - <<'PYEOF'
import numpy as np, OpenEXR, Imath
def load(p):
    f=OpenEXR.InputFile(p); dw=f.header()['dataWindow']
    sz=(dw.max.y-dw.min.y+1,dw.max.x-dw.min.x+1)
    ch=f.channels(['R','G','B'],Imath.PixelType(Imath.PixelType.FLOAT))
    return np.stack([np.frombuffer(c,np.float32).reshape(sz) for c in ch],-1)
OUT="results/campaign/s4_erfinv"
f32=np.mean([load(f"{OUT}/f32_s{s}.exr") for s in (1,2)],0)
hp =np.mean([load(f"{OUT}/hiprec_s{s}.exr") for s in (1,2)],0)
g=np.abs(f32-hp).mean(-1); h,w=g.shape
cen=g[h//2-h//8:h//2+h//8,w//2-w//8:w//2+w//8]
print("S4 -- float32 erfinvf vs double erfinv (cluster, albedo 0.9, sigma 2, 16384 spp, mean of 2 seeds):")
print(f"  mean f32={f32.mean():.6f}  hiprec={hp.mean():.6f}  mean diff={f32.mean()-hp.mean():+.2e}")
print(f"  |diff|: image mean={g.mean():.2e}  centre(1/4) mean={cen.mean():.2e}  max={g.max():.2e}")
v=cen.mean()
print(f"  verdict: erf^-1 precision contributes ~{v:.0e} at the overlap -> "
      f"{'EXPLAINS the +2e-4 residual' if v>1e-4 else 'does NOT explain the +2e-4 residual (independence/containment effect)'}")
PYEOF
echo "=== S4 DONE $(date -u) ==="
