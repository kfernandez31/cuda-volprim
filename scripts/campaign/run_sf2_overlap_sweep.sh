#!/usr/bin/env bash
# SF2 (Condor round-2): measure the NEE furnace over-count vs OVERLAP at fixed sigma, to turn the
# single-Gaussian-thickness -> +156% bridge from argued into measured. Mitsuba-NEE furnace (albedo=1,
# white-constant) on the cloud (overlap ~45) vs the banked single Gaussian (overlap 1). Clock-independent.
set -uo pipefail
cd /home/kacper/thesis
OUT=results/campaign/furnace; mkdir -p "$OUT"
PY=tools/refs/.venv/bin/python; M=tools/refs/with_jorge_mitsuba.sh
for S in 6 7.5; do
  echo "=== cloud furnace NEE (albedo=1, white-constant, sigma=$S, overlap ~45) ==="
  SG_ALBEDO=1.0 SG_ENV=white_constant SG_NEE=1 SG_SIGMA=$S SG_SPP=512 SG_CAM=0 \
    $M $PY tools/refs/render_cloud_prb_absorption.py 2>&1 | grep -E "cam_0000|mean" | head -2
  cp "$(ls -t assets/models/cloud/refs_prb_scattering*nee*/0000.exr | head -1)" "$OUT/mits_nee_cloudfurnace_s${S}.exr"
done
$PY - <<'PYEOF'
import numpy as np, OpenEXR, Imath
def load(p):
    f=OpenEXR.InputFile(p); dw=f.header()['dataWindow']
    sz=(dw.max.y-dw.min.y+1,dw.max.x-dw.min.x+1)
    ch=f.channels(['R','G','B'],Imath.PixelType(Imath.PixelType.FLOAT))
    return np.stack([np.frombuffer(c,np.float32).reshape(sz) for c in ch],-1)
def centre(p):
    a=load(p); g=a.mean(-1); h,w=g.shape
    return a.mean(), g[h//2-h//8:h//2+h//8, w//2-w//8:w//2+w//8].mean()
print("NEE furnace over-count vs overlap, fixed sigma (centre = mean over central 1/4 box):")
sm,sc=centre("results/campaign/furnace/mits_nee_furnace_s6.exr")
print(f"  single Gaussian (overlap 1),  sigma 6  : image {(sm-1)*100:+.2f}%  centre {(sc-1)*100:+.2f}%")
for S in (6,7.5):
    try: m,c=centre(f"results/campaign/furnace/mits_nee_cloudfurnace_s{S}.exr")
    except Exception as e: print(f"  cloud sigma {S}: (missing: {e})"); continue
    print(f"  cloud (overlap ~45),          sigma {S}: image {(m-1)*100:+.2f}%  centre {(c-1)*100:+.2f}%")
PYEOF
echo "=== SF2 SWEEP DONE $(date -u) ==="
