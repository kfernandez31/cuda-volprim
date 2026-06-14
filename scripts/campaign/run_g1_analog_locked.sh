#!/usr/bin/env bash
# G1 comparison A, RE-TIMED at locked clocks (1800). Re-times ours-analog + Mitsuba-analog (render
# time), reuses banked 16-seed images for variance, computes raw + firefly-clipped equal-quality
# speedup with a CONSISTENT clip on both arms. Writes g1_analog_final.md + status marker.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
exec > >(tee results/campaign/g1_analog_locked.log) 2>&1
echo "=== start $(date) ==="
rm -f results/campaign/.g1_analog_locked.status
CLK=results/campaign/clk_g1analog_locked.log
nvidia-smi --query-gpu=clocks.sm --format=csv,noheader,nounits -lms 200 > "$CLK" & SENT=$!; trap 'kill $SENT 2>/dev/null' EXIT

echo "--- ours-analog timing (5 renders, locked) ---"
cp ~/winbins/exe_analog build/bin/Release/test_runner; cp ~/winbins/ir_analog build/device_program.optixir
OT=""
for S in 0 1 2 3 4; do
  t=$(SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering --sigma-multiplier 7.5 --spp 64 --seed "$S" 2>&1 | grep -oE "Total time: [0-9.]+s" | grep -oE "[0-9.]+")
  OT="$OT $t"; echo "ours-analog seed $S: ${t}s"
done
cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir

echo "--- Mitsuba-analog timing (4 renders, locked, RENDER_TIME_S) ---"
MT=""
for S in 0 1 2 3; do
  t=$(SG_ENV=meadow SG_CAM=0 SG_ALBEDO=0.9 SG_SIGMA=7.5 SG_SPP=64 SG_SEED="$S" SG_HG_G=0.85 SG_NEE=0 \
        tools/refs/with_jorge_mitsuba.sh tools/refs/.venv/bin/python tools/refs/render_cloud_prb_absorption.py 2>&1 \
        | grep -oE "RENDER_TIME_S [0-9.]+" | grep -oE "[0-9.]+" | head -1)
  MT="$MT $t"; echo "mits-analog seed $S: ${t}s"
done
echo "clk: $(sort -n "$CLK" | awk 'NR==1{m=$1}{a[NR]=$1}END{print "min="m" p50="a[int(NR/2)]" max="a[NR]}')"

tools/refs/.venv/bin/python - "$OT" "$MT" <<'PY2'
import sys, glob, numpy as np, OpenEXR, Imath, statistics as st
def load(p):
    f=OpenEXR.InputFile(p); dw=f.header()['dataWindow']
    w=dw.max.x-dw.min.x+1; h=dw.max.y-dw.min.y+1; pt=Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c,pt),np.float32).reshape(h,w) for c in('R','G','B')],-1)
def ks(pat, spp=64, pct=99.9):
    A=np.array([load(f) for f in sorted(glob.glob(pat))])
    kraw=float(A.var(0,ddof=1).mean()*spp)
    P=np.percentile(A,pct); kc=float(np.minimum(A,P).var(0,ddof=1).mean()*spp)
    return kraw,kc
ot=[float(x) for x in sys.argv[1].split()]; mt=[float(x) for x in sys.argv[2].split()]
oraw,oclip=ks('results/campaign/g1_analog_seeds/analog_seed*.exr')
mraw,mclip=ks('results/campaign/g1_seeds/mits_seed*.exr')
otm=st.median(ot); mtm=st.median(mt)
out=[]
out.append(f"ours-analog : k_raw={oraw:.0f} k_clip999={oclip:.1f} t_med={otm:.2f}s (locked)")
out.append(f"Mits-analog : k_raw={mraw:.0f} k_clip999={mclip:.1f} t_med={mtm:.2f}s (locked)")
out.append(f"per-sample time ratio (mits/ours) = {mtm/otm:.2f}x")
out.append(f"equal-quality speedup RAW   (k*t mits/ours) = {(mraw*mtm)/(oraw*otm):.2f}x")
out.append(f"equal-quality speedup CLIP  (k*t mits/ours) = {(mclip*mtm)/(oclip*otm):.2f}x")
print("\n".join(out))
open('results/campaign/g1_analog_final.md','w').write("# G1 comparison A — re-timed at locked clocks (1800)\n\n"+"\n".join("- "+l for l in out)+"\n\nConsistent firefly-clip (99.9pct pooled) on both arms; variance from banked 16-seed images; times re-measured at pinned 1800MHz.\n")
PY2
echo "=== done $(date) ==="
echo DONE > results/campaign/.g1_analog_locked.status
