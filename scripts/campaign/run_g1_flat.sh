#!/usr/bin/env bash
# G1 FLAT rung: ours-MIS vs Mitsuba-analog on a FLAT (white-constant) environment, cloud.
# Bounds how much of the meadow 59x is environment importance sampling: on a flat env there is
# no env peakiness for our MIS to exploit AND no bright-sun fireflies to plague Mitsuba-analog,
# so this isolates the residual sampler/architecture advantage. 8 seeds @64 spp, sigma 7.5,
# albedo 0.9, HG g=0.85, cloud native res. Locked clocks (>=300 W). Writes g1_flat.md.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
TS=$(date +%H%M); exec > >(tee results/campaign/g1_flat_${TS}.log) 2>&1
echo "=== G1 flat start $(date) ==="
rm -f results/campaign/.g1flat.status
PW=$(nvidia-smi --query-gpu=power.limit --format=csv,noheader,nounits|cut -d. -f1)
[ "${PW:-0}" -lt 300 ] && { echo "ABORT power ${PW}W<300"; echo FAIL > results/campaign/.g1flat.status; exit 1; }
OUT=results/campaign/g1_flat_seeds; mkdir -p "$OUT"
grep -qxF "results/campaign/g1_flat_seeds/" .gitignore || echo "results/campaign/g1_flat_seeds/" >> .gitignore
MITS_DIR="assets/models/cloud/refs_prb_scattering_hg0.85"   # flat env, scattering, hg0.85, analog
CLK=results/campaign/clk_g1flat_${TS}.log
nvidia-smi --query-gpu=clocks.sm --format=csv,noheader,nounits -lms 200 > "$CLK" & SENT=$!; trap 'kill $SENT 2>/dev/null' EXIT
now(){ date +%s.%N; }
echo "seed,ours_total_s,mits_render_s" > "$OUT/times.csv"

cp ~/winbins/exe_cloud build/bin/Release/test_runner; cp ~/winbins/ir_cloud build/device_program.optixir
for S in $(seq 0 7); do
  # OURS (MIS, flat env)
  o=$(SG_ENV=white_constant SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering \
        --sigma-multiplier 7.5 --spp 64 --seed "$S" 2>&1)
  grep -q "Cap overflow:" <<<"$o" && echo "!!! OVERFLOW ours seed $S"
  ot=$(grep -oE "Total time: [0-9.]+s" <<<"$o" | grep -oE "[0-9.]+")
  cp test_results/cloud_asset_scattering/0000.exr "$OUT/ours_seed$(printf %02d "$S").exr"
  # MITSUBA-analog (NEE off, flat env)
  m=$(SG_ENV=white_constant SG_CAM=0 SG_ALBEDO=0.9 SG_SIGMA=7.5 SG_SPP=64 SG_SEED="$S" SG_HG_G=0.85 SG_NEE=0 \
        experiments/mitsuba-reference/with_jorge_mitsuba.sh experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/render_cloud_prb_absorption.py 2>&1)
  mr=$(grep -oE "RENDER_TIME_S [0-9.]+" <<<"$m" | grep -oE "[0-9.]+" | head -1)
  cp "$MITS_DIR/0000.exr" "$OUT/mits_seed$(printf %02d "$S").exr" 2>/dev/null || echo "WARN no mits EXR seed $S"
  echo "$S,$ot,${mr:-NA}" | tee -a "$OUT/times.csv"
done
cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir
echo "clock: $(sort -n "$CLK"|awk 'NR==1{m=$1}{a[NR]=$1}END{print "min="m" p50="a[int(NR/2)]" max="a[NR]}')"

experiments/mitsuba-reference/.venv/bin/python - <<'PY'
import OpenEXR, Imath, numpy as np, glob, csv, statistics as st
def load(p):
    f=OpenEXR.InputFile(p); dw=f.header()['dataWindow']
    w=dw.max.x-dw.min.x+1; h=dw.max.y-dw.min.y+1; pt=Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c,pt),np.float32).reshape(h,w) for c in('R','G','B')],-1)
def ks(pat, spp=64):
    A=np.array([load(f) for f in sorted(glob.glob(pat))])
    kraw=float(A.var(0,ddof=1).mean()*spp)
    P=np.percentile(A,99.9); kc=float(np.minimum(A,P).var(0,ddof=1).mean()*spp)
    return kraw,kc,float(A.mean()),len(A)
oraw,oclip,omean,no=ks('results/campaign/g1_flat_seeds/ours_seed*.exr')
mraw,mclip,mmean,nm=ks('results/campaign/g1_flat_seeds/mits_seed*.exr')
rows=[r for r in csv.DictReader(open('results/campaign/g1_flat_seeds/times.csv'))]
ot=[float(r['ours_total_s']) for r in rows if r['ours_total_s'] not in('','NA')]
mt=[float(r['mits_render_s']) for r in rows if r['mits_render_s'] not in('','NA')]
otm=st.median(ot) if ot else float('nan'); mtm=st.median(mt) if mt else float('nan')
L=[]
L.append(f"ours-MIS (flat)   : mean={omean:.4f} k_raw={oraw:.2f} k_clip999={oclip:.2f} t_med={otm:.2f}s (n={no})")
L.append(f"Mitsuba-analog(flat): mean={mmean:.4f} k_raw={mraw:.2f} k_clip999={mclip:.2f} t_med={mtm:.2f}s (n={nm})")
L.append(f"mean ratio ours/mits = {omean/mmean:.4f}")
if otm==otm and mtm==mtm:
    L.append(f"equal-quality speedup RAW  (k*t mits/ours) = {(mraw*mtm)/(oraw*otm):.2f}x")
    L.append(f"equal-quality speedup CLIP (k*t mits/ours) = {(mclip*mtm)/(oclip*otm):.2f}x")
print("\n".join(L))
open('results/campaign/g1_flat.md','w').write(
 "# G1 FLAT rung — ours-MIS vs Mitsuba-analog on a FLAT (white-constant) env\n\n"
 +"\n".join("- "+l for l in L)
 +"\n\nFlat env removes both our env-IS advantage and Mitsuba-analog's bright-sun fireflies, so this\n"
  "isolates the residual sampler/architecture speedup vs the meadow ~59x (which is env-IS-dominated).\n")
PY
echo "=== G1 flat done $(date) ==="
echo DONE > results/campaign/.g1flat.status
