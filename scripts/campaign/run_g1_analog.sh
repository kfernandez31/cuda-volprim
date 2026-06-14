#!/usr/bin/env bash
# G1 comparison A: ours-ANALOG vs Mitsuba-ANALOG (same estimator class both sides) on cloud-meadow.
# Isolates the core free-flight sampler (argmin vs marching+root-find) from the NEE/MIS difference.
# Reuses the banked Mitsuba-analog seeds (g1_seeds/mits_seed*). Clocks logged. 350 W.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
exec > >(tee results/campaign/g1_analog.log) 2>&1
echo "=== G1 comparison A start $(date) ==="
rm -f results/campaign/.g1_analog.status
OUT=results/campaign/g1_analog_seeds; mkdir -p "$OUT"
grep -qxF "results/campaign/g1_analog_seeds/" .gitignore || echo "results/campaign/g1_analog_seeds/" >> .gitignore
cp ~/winbins/exe_analog build/bin/Release/test_runner; cp ~/winbins/ir_analog build/device_program.optixir
CLK=results/campaign/clk_g1analog.log
nvidia-smi --query-gpu=clocks.sm --format=csv,noheader,nounits -lms 250 > "$CLK" & SENT=$!; trap 'kill $SENT 2>/dev/null' EXIT
echo "seed,ours_analog_s" > "$OUT/times.csv"
for S in $(seq 0 15); do
  o=$(SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering \
        --sigma-multiplier 7.5 --spp 64 --seed "$S" 2>&1)
  grep -q "Cap overflow:" <<<"$o" && echo "!!! OVERFLOW seed $S"
  t=$(grep -oE "Total time: [0-9.]+s" <<<"$o" | grep -oE "[0-9.]+")
  cp test_results/cloud_asset_scattering/0000.exr "$OUT/analog_seed$(printf %02d "$S").exr"
  echo "$S,$t" | tee -a "$OUT/times.csv"
done
cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir
echo "clk: $(sort -n "$CLK" | awk 'NR==1{m=$1}{a[NR]=$1}END{print "min="m" p50="a[int(NR/2)]" max="a[NR]}')"

echo "--- clipped-k + speedup (ours-analog vs Mitsuba-analog) ---"
tools/refs/.venv/bin/python - <<'PY'
import OpenEXR, Imath, numpy as np, glob, csv, statistics as st
def load(p):
    f=OpenEXR.InputFile(p); dw=f.header()['dataWindow']
    w=dw.max.x-dw.min.x+1; h=dw.max.y-dw.min.y+1; pt=Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c,pt),np.float32).reshape(h,w) for c in('R','G','B')],-1)
def kvals(glob_pat, spp=64):
    fs=sorted(glob.glob(glob_pat));
    if len(fs)<2: return None
    A=np.array([load(f) for f in fs])                      # (S,H,W,3)
    var=A.var(axis=0, ddof=1).mean(-1)                     # per-pixel inter-seed var (avg channels)
    k=var*spp
    kbar=float(k.mean())
    kclip=float(np.clip(k, None, np.percentile(k,99.9)).mean())
    return kbar, kclip, A.mean(0).mean()
oa=kvals('results/campaign/g1_analog_seeds/analog_seed*.exr')
ma=kvals('results/campaign/g1_seeds/mits_seed*.exr')
# times
ot=[float(r['ours_analog_s']) for r in csv.DictReader(open('results/campaign/g1_analog_seeds/times.csv')) if r['ours_analog_s']]
try:
    mt=[float(r['mits_render_s']) for r in csv.DictReader(open('results/campaign/g1_seeds/cloud_times.csv')) if r.get('mits_render_s','NA') not in('NA','')]
except Exception: mt=[]
ot_med=st.median(ot) if ot else float('nan'); mt_med=st.median(mt) if mt else float('nan')
print(f"ours-analog : k={oa[0]:.1f} k_clip999={oa[1]:.2f} mean={oa[2]:.4f} t_med={ot_med:.2f}s (n={len(ot)})")
print(f"Mitsuba-analog: k={ma[0]:.1f} k_clip999={ma[1]:.2f} mean={ma[2]:.4f} t_med={mt_med:.2f}s")
if oa and ma and ot_med==ot_med and mt_med==mt_med:
    sp_clip=(ma[1]*mt_med)/(oa[1]*ot_med)
    sp_raw=(ma[0]*mt_med)/(oa[0]*ot_med)
    print(f"core-sampler speedup (analog vs analog): clipped {sp_clip:.2f}x  raw {sp_raw:.1f}x")
PY
echo "=== G1 comparison A done $(date) ==="
echo DONE > results/campaign/.g1_analog.status
