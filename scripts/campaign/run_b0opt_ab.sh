#!/usr/bin/env bash
# Bounce-0 precompute A/B: per-asset interleaved stock-vs-opt at the tab:asset-cost
# recipe (scattering, albedo 0.9, white env, sigma 10, 512x512, 64 spp), per-asset
# calibrated caps, warm-up + 5 retained repeats per arm. Requires the 8 binaries from
# build_b0opt_pairs.sh and the locked operating point (>=300 W; §5.1 protocol).
# Resumable: rows are appended only when complete; re-running skips finished rows.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
TS=$(date +%H%M)
exec > >(tee results/campaign/b0opt_ab_${TS}.log) 2>&1
echo "=== b0opt A/B start $(date) ==="

PW=$(nvidia-smi --query-gpu=power.limit --format=csv,noheader,nounits | cut -d. -f1)
[ "${PW:-0}" -lt 300 ] && { echo "ABORT power ${PW}W<300 (lock clocks first)"; exit 1; }
nvidia-smi --query-gpu=clocks.sm --format=csv,noheader,nounits -lms 250 \
  > results/campaign/clk_b0opt_${TS}.log & SENT=$!
cleanup() {
  [ -n "${SENT:-}" ] && kill "$SENT" 2>/dev/null
  cp ~/winbins/exe_stock build/bin/Release/test_runner 2>/dev/null
  cp ~/winbins/ir_stock build/device_program.optixir 2>/dev/null
}
trap cleanup EXIT

declare -A PLY=( [cloud]=assets/models/cloud/root.primitives_pyr0.ply
                 [tornado]=assets/models/tornado/tornado_pyr0.ply
                 [explosion]=assets/models/explosion/explosion_pyr0.ply
                 [bunny]=assets/models/bunny/bunny_pyr0.ply )

tt() { grep -oE "Total time: [0-9.]+" | grep -oE "[0-9.]+" | head -1; }
CSV=results/campaign/b0opt_ab.csv
[ -f "$CSV" ] || echo "asset,arm,t0,t1,t2,t3,t4,t_med_s,identical_to_ref" > "$CSV"

run_arm() {  # $1 asset  $2 arm(stock|opt)
  grep -q "^$1,$2," "$CSV" && { echo "skip $1/$2 (done)"; return 0; }
  local tag="b0${2}_$1"
  [ -f ~/winbins/exe_$tag ] || { echo "ABORT: exe_$tag missing (run build_b0opt_pairs.sh)"; exit 1; }
  cp ~/winbins/exe_$tag build/bin/Release/test_runner
  cp ~/winbins/ir_$tag build/device_program.optixir
  local common=(--scene asset_validation --spp 64 --sigma-multiplier 10 --seed 0)
  SG_PLY="${PLY[$1]}" SG_RES=512 SG_ENV=white_constant SG_ALBEDO=0.9 \
    build/bin/Release/test_runner "${common[@]}" > /dev/null 2>&1   # warm-up
  local T=() t o
  for r in 1 2 3 4 5; do
    o=$(SG_PLY="${PLY[$1]}" SG_RES=512 SG_ENV=white_constant SG_ALBEDO=0.9 \
        build/bin/Release/test_runner "${common[@]}" 2>&1)
    t=$(tt <<<"$o"); T+=("$t"); echo "  $1/$2 repeat $r: ${t}s"
  done
  # correctness gate folded in: seed-0 image must match the asset's stock reference
  local ident=""
  if [ -f "results/campaign/b0opt_refs/$1.exr" ]; then
    cmp -s test_results/asset_validation/0000.exr "results/campaign/b0opt_refs/$1.exr" \
      && ident=1 || ident=0
  fi
  local med
  med=$(printf '%s\n' "${T[@]}" | sort -n | sed -n '3p')
  echo "$1,$2,${T[0]},${T[1]},${T[2]},${T[3]},${T[4]},$med,$ident" >> "$CSV"
  echo "$1/$2 med=${med}s identical=$ident"
}

mkdir -p results/campaign/b0opt_refs
for a in cloud tornado explosion bunny; do
  # bank the stock seed-0 reference image once per asset (from the stock arm's last run)
  run_arm "$a" stock
  [ -f "results/campaign/b0opt_refs/$a.exr" ] || \
    cp test_results/asset_validation/0000.exr "results/campaign/b0opt_refs/$a.exr"
  run_arm "$a" opt
done
echo "=== b0opt A/B done $(date) ==="