#!/usr/bin/env bash
# G1 headline core: cloud-meadow equal-quality, ours-final (analytic, MIS, calibrated caps) vs
# Mitsuba-analog (NEE off), 16 seeds @64 spp, sigma 7.5, resolution-matched (cloud native 900x600).
# Captures EXRs (k + triptych) + render times. Bunny = ours-internal (separate). REQUIRES >=300 W.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
TS=$(date +%H%M)
exec > >(tee results/campaign/g1_${TS}.log) 2>&1
echo "=== G1 start $(date) ==="
PW=$(nvidia-smi --query-gpu=power.limit --format=csv,noheader,nounits | cut -d. -f1)
[ "${PW:-0}" -lt 300 ] && { echo "ABORT power ${PW}W<300"; echo FAIL > results/campaign/.g1.status; exit 1; }
cp ~/winbins/exe_cloud build/bin/Release/test_runner; cp ~/winbins/ir_cloud build/device_program.optixir
OUT=results/campaign/g1_seeds; mkdir -p "$OUT"
grep -qxF "results/campaign/g1_seeds/" .gitignore || echo "results/campaign/g1_seeds/" >> .gitignore
MITS_DIR="assets/models/cloud/refs_prb_scattering_meadow_hg0.85"
declare -i OVF=0
nvidia-smi --query-gpu=clocks.sm --format=csv,noheader,nounits -lms 200 > results/campaign/clk_g1_${TS}.log & SENT=$!; trap 'kill $SENT 2>/dev/null' EXIT
now(){ date +%s.%N; }
echo "seed,ours_total_s,mits_render_s,mits_wall_s" > "$OUT/cloud_times.csv"

echo "--- cloud-meadow, 16 seeds @64 spp, sigma 7.5 ---"
for S in $(seq 0 15); do
  # OURS (MIS, analytic, calibrated caps)
  o=$(SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering \
        --sigma-multiplier 7.5 --spp 64 --seed "$S" 2>&1)
  grep -q "Cap overflow:" <<<"$o" && { OVF+=1; echo "!!! OVERFLOW ours seed $S"; }
  ot=$(grep -oE "Total time: [0-9.]+s" <<<"$o" | grep -oE "[0-9.]+")
  cp test_results/cloud_asset_scattering/0000.exr "$OUT/cuda_seed$(printf %02d "$S").exr"
  # MITSUBA-analog (NEE off)
  w0=$(now)
  m=$(SG_ENV=meadow SG_CAM=0 SG_ALBEDO=0.9 SG_SIGMA=7.5 SG_SPP=64 SG_SEED="$S" SG_HG_G=0.85 SG_NEE=0 \
        tools/refs/with_jorge_mitsuba.sh tools/refs/.venv/bin/python tools/refs/render_cloud_prb_absorption.py 2>&1)
  w1=$(now)
  mr=$(grep -oE "RENDER_TIME_S [0-9.]+" <<<"$m" | grep -oE "[0-9.]+" | head -1)
  mw=$(python3 -c "print(f'{$w1-$w0:.3f}')")
  cp "$MITS_DIR/0000.exr" "$OUT/mits_seed$(printf %02d "$S").exr" 2>/dev/null || echo "WARN: no mits EXR seed $S"
  echo "$S,$ot,${mr:-NA},$mw" | tee -a "$OUT/cloud_times.csv"
done
cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir
echo "clock: $(sort -n results/campaign/clk_g1_${TS}.log | awk 'NR==1{m=$1}{a[NR]=$1}END{print "min="m" p50="a[int(NR/2)]" max="a[NR]}')"
echo "OVERFLOWS: $OVF"
echo "=== G1 cloud done $(date) ==="
echo DONE > results/campaign/.g1.status
