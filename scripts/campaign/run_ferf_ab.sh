#!/usr/bin/env bash
# G2b — fast-erf A/B: exact-erf (cloud calibrated 64/96) vs fast-erf (build-ferf, 64/96).
# fast-erf is numerically ~identical (~5e-7) so k is unchanged → the win is per-spp TIME.
# Interleaved (per-round ratio cancels thermal drift) + same-seed converged bias check.
# REQUIRES >=300 W. Overflow-checked. Headless: setsid nohup bash ... </dev/null &
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
TS=$(date +%H%M)
exec > >(tee results/campaign/ferf_ab_${TS}.log) 2>&1
echo "=== fast-erf A/B start $(date) ==="
PW=$(nvidia-smi --query-gpu=power.limit --format=csv,noheader,nounits | cut -d. -f1)
[ "${PW:-0}" -lt 300 ] && { echo "ABORT power ${PW}W<300"; echo FAIL > results/campaign/.ferf.status; exit 1; }

EXACT=build/bin/Release/test_runner          # cloud calibrated, exact erf
FERF=build-ferf/bin/Release/test_runner      # fast erf, runs in place (R3: baked optixir path)
cp ~/winbins/exe_cloud "$EXACT"; cp ~/winbins/ir_cloud build/device_program.optixir

declare -i OVF=0
chk(){ grep -q "Cap overflow:" <<<"$1" && { OVF+=1; echo "!!! OVERFLOW: $(grep 'Cap overflow:' <<<"$1"|head -1)"; }; }
t_of(){ grep -oE "Total time: [0-9.]+s" <<<"$1" | grep -oE "[0-9.]+"; }

nvidia-smi --query-gpu=clocks.sm --format=csv,noheader,nounits -lms 200 > results/campaign/clk_ferf_${TS}.log &
SENT=$!; trap 'kill $SENT 2>/dev/null' EXIT

echo "--- interleaved time A/B (meadow 64spp seed 1, 8 rounds) ---"
LOG=results/campaign/ferf_rounds_${TS}.log; : > $LOG
for r in $(seq 1 8); do
  oe=$(SG_ENV=meadow SG_CAM=0 $EXACT --scene cloud_asset_scattering --spp 64 --seed 1 2>&1); chk "$oe"; te=$(t_of "$oe")
  of=$(SG_ENV=meadow SG_CAM=0 $FERF  --scene cloud_asset_scattering --spp 64 --seed 1 2>&1); chk "$of"; tf=$(t_of "$of")
  echo "round=$r exact=$te ferf=$tf" | tee -a $LOG
done

echo "--- bias check (meadow 1024spp seed 99, SAME seed → diff = pure erf error) ---"
SG_ENV=meadow SG_CAM=0 $EXACT --scene cloud_asset_scattering --spp 1024 --seed 99 2>&1 | grep -E "Cap overflow" || true
cp test_results/cloud_asset_scattering/0000.exr results/campaign/ferf_exact_1024.exr
SG_ENV=meadow SG_CAM=0 $FERF  --scene cloud_asset_scattering --spp 1024 --seed 99 2>&1 | grep -E "Cap overflow" || true
cp test_results/cloud_asset_scattering/0000.exr results/campaign/ferf_fast_1024.exr
tools/refs/.venv/bin/python tools/refs/exr_diff.py results/campaign/ferf_exact_1024.exr results/campaign/ferf_fast_1024.exr \
  | tee results/campaign/ferf_bias_${TS}.txt

tools/refs/.venv/bin/python - "$LOG" <<'PY'
import sys, statistics as st, re
e=[];f=[]
for line in open(sys.argv[1]):
    m=re.search(r'exact=([0-9.]+) ferf=([0-9.]+)', line)
    if m: e.append(float(m.group(1))); f.append(float(m.group(2)))
ratios=[a/b for a,b in zip(e,f)]
print(f"exact median {st.median(e):.3f}s   ferf median {st.median(f):.3f}s")
print(f"median per-round ratio (exact/ferf) = {st.median(ratios):.4f}  ({100*(st.median(ratios)-1):+.1f}% faster, n={len(ratios)})")
PY

echo "clock: $(sort -n results/campaign/clk_ferf_${TS}.log | awk 'NR==1{m=$1}{a[NR]=$1}END{print "min="m" p50="a[int(NR/2)]" max="a[NR]}')"
echo "OVERFLOWS: $OVF"
cp ~/winbins/exe_stock "$EXACT"; cp ~/winbins/ir_stock build/device_program.optixir   # restore canonical
echo "=== fast-erf A/B done $(date) ==="
[ "$OVF" -eq 0 ] && echo DONE > results/campaign/.ferf.status || echo DONE_WITH_OVERFLOWS > results/campaign/.ferf.status
