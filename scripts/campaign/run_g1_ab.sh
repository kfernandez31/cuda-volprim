#!/usr/bin/env bash
# G1 A+B. A: ours-analog vs Mitsuba-analog (core speed). GT: the two analogs agree. B: ours-NEE vs
# Mitsuba-NEE vs analog GT (Mitsuba-NEE biased). Build ours-analog (64/96 + ENABLE_NEE=false). >=300W.
set -uo pipefail; cd "$(git rev-parse --show-toplevel)"
TS=$(date +%H%M); exec > >(tee results/campaign/g1ab_${TS}.log) 2>&1
echo "=== G1 A/B start $(date) ==="
PW=$(nvidia-smi --query-gpu=power.limit --format=csv,noheader,nounits|cut -d. -f1)
[ "${PW:-0}" -lt 300 ] && { echo "ABORT power<300"; echo FAIL>results/campaign/.g1ab.status; exit 1; }
# --- build ours-analog (caps 64/96 + NEE off) ---
sed -i 's/MAX_ACTIVE_PRIMS = [0-9]*;/MAX_ACTIVE_PRIMS = 64;/;s/HIT_BUFFER_CAPACITY = [0-9]*;/HIT_BUFFER_CAPACITY = 96;/;s/ENABLE_NEE = true;/ENABLE_NEE = false;/' device/core/constants.cuh
echo "build cfg: $(grep -oE 'ENABLE_NEE = (true|false)|MAX_ACTIVE_PRIMS = [0-9]+|HIT_BUFFER_CAPACITY = [0-9]+' device/core/constants.cuh|tr '\n' ' ')"
cmake --build build --target test_runner -j"$(nproc)" >/dev/null 2>&1 && echo "ours-analog built"
git checkout -- device/core/constants.cuh
OUT=results/campaign/g1_seeds; mkdir -p "$OUT"; declare -i OVF=0
nvidia-smi --query-gpu=clocks.sm --format=csv,noheader,nounits -lms 200 > results/campaign/clk_g1ab_${TS}.log & SENT=$!; trap 'kill $SENT 2>/dev/null' EXIT
echo "arm,seed,total_s" > "$OUT/ab_times.csv"
# --- A: ours-analog (build/ holds it now), 16 seeds ---
echo "--- ours-analog (A + GT), 16 seeds @64spp sigma7.5 meadow ---"
for S in $(seq 0 15); do
  o=$(SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering --sigma-multiplier 7.5 --spp 64 --seed $S 2>&1)
  grep -q "Cap overflow:" <<<"$o" && OVF+=1
  t=$(grep -oE "Total time: [0-9.]+s" <<<"$o"|grep -oE "[0-9.]+")
  cp test_results/cloud_asset_scattering/0000.exr "$OUT/ouranalog_seed$(printf %02d $S).exr"
  echo "ours_analog,$S,$t" | tee -a "$OUT/ab_times.csv"
done
# --- B: Mitsuba-NEE, 16 seeds (note _nee output dir) ---
echo "--- Mitsuba-NEE (B), 16 seeds @64spp sigma7.5 meadow ---"
MNEE="assets/models/cloud/refs_prb_scattering_meadow_hg0.85_nee"
for S in $(seq 0 15); do
  SG_ENV=meadow SG_CAM=0 SG_ALBEDO=0.9 SG_SIGMA=7.5 SG_SPP=64 SG_SEED=$S SG_HG_G=0.85 SG_NEE=1 \
    experiments/mitsuba-reference/with_jorge_mitsuba.sh experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/render_cloud_prb_absorption.py >/dev/null 2>&1
  cp "$MNEE/0000.exr" "$OUT/mitsnee_seed$(printf %02d $S).exr" 2>/dev/null || echo "WARN no mits-nee seed $S"
  echo "mits_nee,$S,NA" >> "$OUT/ab_times.csv"
done
cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir
echo "OVERFLOWS: $OVF"; echo "=== G1 A/B done $(date) ==="; echo DONE > results/campaign/.g1ab.status
