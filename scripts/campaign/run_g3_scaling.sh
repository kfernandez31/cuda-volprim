#!/usr/bin/env bash
# G3/R3 — scaling study: render TIME and device MEMORY vs primitive count N.
#   A) Real-asset sweep (4 assets, each its OWN calibrated binary): matched config
#      scattering albedo 0.9, white_constant env, 512^2, diag view, sigma 10, 64 spp.
#      N = cloud 652 / tornado 768 / explosion 1024 / bunny 25600. Median of 3 seeds.
#   B) Synthetic stress grids (SAFE-512 binary => CONSTANT caps, only N varies):
#      stress_{16,256,512,1024,2048,4096,8192}_gaussians, 512^2 / 64 spp. Median of 3.
#      Isolates N: same primitive kind, same caps, same launch -> clean time(N).
#   C) Peak VRAM at the synthetic endpoints (N=16 and N=8192, SAFE-512) -> confirms
#      memory is decoupled from N (flat, cap-dominated). GAS(N) from gas_memory.csv.
# Locked clocks (1800/9751, >=300 W). Writes results/campaign/scaling.{md,csv}.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
TS=$(date +%H%M); exec > >(tee results/campaign/g3_scaling_${TS}.log) 2>&1
echo "=== G3 scaling start $(date) ==="
rm -f results/campaign/.g3scaling.status
PW=$(nvidia-smi --query-gpu=power.limit --format=csv,noheader,nounits|cut -d. -f1)
[ "${PW:-0}" -lt 300 ] && { echo "ABORT power ${PW}W<300"; echo FAIL > results/campaign/.g3scaling.status; exit 1; }
CLK=results/campaign/clk_g3scaling_${TS}.log
nvidia-smi --query-gpu=clocks.sm --format=csv,noheader,nounits -lms 250 > "$CLK" & SENT=$!; trap 'kill $SENT 2>/dev/null' EXIT

tt(){ grep -oE "Total time: [0-9.]+s" | grep -oE "[0-9.]+" | head -1; }   # parse seconds
med3(){ printf '%s\n' "$@" | sort -n | sed -n '2p'; }                      # median of 3

CSV=results/campaign/scaling.csv
echo "kind,label,N,t_med_s,t0,t1,t2,peak_mib" > "$CSV"

echo "===================== A) REAL-ASSET MATCHED SWEEP ====================="
declare -A PLY=( [cloud]=assets/models/cloud/root.primitives_pyr0.ply
                 [tornado]=assets/models/tornado/tornado_pyr0.ply
                 [explosion]=assets/models/explosion/explosion_pyr0.ply
                 [bunny]=assets/models/bunny/bunny_pyr0.ply )
declare -A NP=( [cloud]=652 [tornado]=768 [explosion]=1024 [bunny]=25600 )
for a in cloud tornado explosion bunny; do
  cp ~/winbins/exe_$a build/bin/Release/test_runner; cp ~/winbins/ir_$a build/device_program.optixir
  T=()
  for S in 0 1 2; do
    o=$(SG_PLY=${PLY[$a]} SG_ENV=white_constant SG_ALBEDO=0.9 SG_RES=512 SG_VIEW=diag \
         build/bin/Release/test_runner --scene asset_validation --spp 64 --sigma-multiplier 10 --seed "$S" 2>&1)
    grep -q "Cap overflow:" <<<"$o" && echo "!!! OVERFLOW $a seed $S"
    t=$(tt <<<"$o"); T+=("$t"); echo "  $a (N=${NP[$a]}) seed $S: ${t}s"
  done
  m=$(med3 "${T[@]}")
  echo "asset,$a,${NP[$a]},$m,${T[0]},${T[1]},${T[2]}," >> "$CSV"
  echo "REAL $a N=${NP[$a]} t_med=${m}s"
done

echo "===================== B) SYNTHETIC STRESS SWEEP (SAFE-512) ====================="
cp ~/winbins/exe_safe512 build/bin/Release/test_runner; cp ~/winbins/ir_safe512 build/device_program.optixir
for N in 16 256 512 1024 2048 4096 8192; do
  T=()
  for S in 0 1 2; do
    o=$(build/bin/Release/test_runner --scene stress_${N}_gaussians --width 512 --height 512 --spp 64 --seed "$S" 2>&1)
    grep -q "Cap overflow:" <<<"$o" && echo "!!! OVERFLOW stress_$N seed $S"
    t=$(tt <<<"$o"); T+=("$t"); echo "  stress_$N seed $S: ${t}s"
  done
  m=$(med3 "${T[@]}")
  echo "synthetic,stress_$N,$N,$m,${T[0]},${T[1]},${T[2]}," >> "$CSV"
  echo "SYNTH N=$N t_med=${m}s"
done

echo "===================== C) SYNTHETIC PEAK VRAM (endpoints, SAFE-512) ====================="
poll_peak(){ # $1 scene -> renders while polling per-process used_memory; prints peak MiB
  local scene="$1" pk=0 v
  ( build/bin/Release/test_runner --scene "$scene" --width 512 --height 512 --spp 64 --seed 0 >/dev/null 2>&1 ) & RP=$!
  while kill -0 "$RP" 2>/dev/null; do
    v=$(nvidia-smi --query-compute-apps=used_memory --format=csv,noheader,nounits 2>/dev/null | sort -n | tail -1)
    [ -n "${v:-}" ] && [ "${v:-0}" -gt "$pk" ] 2>/dev/null && pk=$v
    sleep 0.1
  done
  wait "$RP" 2>/dev/null; echo "$pk"
}
for N in 16 8192; do
  pk=$(poll_peak stress_${N}_gaussians)
  echo "SYNTH-VRAM N=$N peak=${pk} MiB (SAFE-512)"
  # patch the peak_mib column into the matching synthetic row
  awk -v n="$N" -v pk="$pk" -F, 'BEGIN{OFS=","} $1=="synthetic"&&$3==n{$8=pk} {print}' "$CSV" > "$CSV.tmp" && mv "$CSV.tmp" "$CSV"
done

cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir
echo "clock: $(sort -n "$CLK"|awk 'NR==1{m=$1}{a[NR]=$1}END{print "min="m" p50="a[int(NR/2)]" max="a[NR]}')"
echo "--- scaling.csv ---"; column -t -s, "$CSV"
echo "=== G3 scaling done $(date) ==="
echo DONE > results/campaign/.g3scaling.status
