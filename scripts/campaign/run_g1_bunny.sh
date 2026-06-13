#!/usr/bin/env bash
# G1 bunny rung (ours-internal): 16 seeds @64spp, meadow, albedo 0.9, calibrated bunny (80/528). >=300W.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
TS=$(date +%H%M); exec > >(tee results/campaign/g1_bunny_${TS}.log) 2>&1
echo "=== G1 bunny start $(date) ==="
PW=$(nvidia-smi --query-gpu=power.limit --format=csv,noheader,nounits|cut -d. -f1)
[ "${PW:-0}" -lt 300 ] && { echo "ABORT power ${PW}W<300"; echo FAIL > results/campaign/.g1bunny.status; exit 1; }
cp ~/winbins/exe_bunny build/bin/Release/test_runner; cp ~/winbins/ir_bunny build/device_program.optixir
OUT=results/campaign/g1_bunny_seeds; mkdir -p "$OUT"
grep -qxF "results/campaign/g1_bunny_seeds/" .gitignore || echo "results/campaign/g1_bunny_seeds/" >> .gitignore
declare -i OVF=0
nvidia-smi --query-gpu=clocks.sm --format=csv,noheader,nounits -lms 200 > results/campaign/clk_g1bunny_${TS}.log & SENT=$!; trap 'kill $SENT 2>/dev/null' EXIT
echo "seed,total_s" > "$OUT/times.csv"
for S in $(seq 0 15); do
  o=$(SG_PLY=assets/models/bunny/bunny_pyr0.ply SG_ENV=meadow SG_ALBEDO=0.9 SG_RES=512 SG_VIEW=diag \
       build/bin/Release/test_runner --scene asset_validation --spp 64 --seed "$S" 2>&1)
  grep -q "Cap overflow:" <<<"$o" && { OVF+=1; echo "!!! OVERFLOW seed $S: $(grep 'Cap overflow:' <<<"$o"|head -1)"; }
  t=$(grep -oE "Total time: [0-9.]+s" <<<"$o"|grep -oE "[0-9.]+")
  cp test_results/asset_validation/0000.exr "$OUT/bunny_seed$(printf %02d "$S").exr"
  echo "$S,$t" | tee -a "$OUT/times.csv"
done
cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir
echo "clock: $(sort -n results/campaign/clk_g1bunny_${TS}.log|awk 'NR==1{m=$1}{a[NR]=$1}END{print "min="m" p50="a[int(NR/2)]" max="a[NR]}')"
echo "OVERFLOWS: $OVF"; echo "=== G1 bunny done $(date) ==="; echo DONE > results/campaign/.g1bunny.status
