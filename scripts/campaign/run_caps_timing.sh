#!/usr/bin/env bash
# B5 — safe-512 vs per-asset-tuned FRAME-TIME penalty (the §4.5 hit-buffer claim).
# Re-banks the four percentages quoted in 04-architecture.tex (previously untraceable).
# Same config as scaling.md part B (tab:asset-cost): scattering albedo 0.9, white_constant env,
# diag view, sigma 10, 512^2, 64 spp. For each asset the TUNED (per-asset exe_<a>) and the
# universal SAFE-512 (exe_safe512, 512/512 caps) builds are timed INTERLEAVED (T,S per round x3,
# binary swapped every render) so same-session GPU drift hits both arms equally (caps_ab.md lesson).
# Penalty = (safe_med - tuned_med)/tuned_med. Identical PLY+scene => identical image; only the
# compile-time caps (=> per-ray local-memory reservation => occupancy) differ.
# Needs locked clocks: scripts/campaign/lock_clocks.sh (>=300 W enforced below).
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
TS=$(date +%H%M); exec > >(tee results/campaign/caps_timing_${TS}.log) 2>&1
echo "=== B5 caps-timing start $(date) ==="
rm -f results/campaign/.capstiming.status
PW=$(nvidia-smi --query-gpu=power.limit --format=csv,noheader,nounits|cut -d. -f1)
[ "${PW:-0}" -lt 300 ] && { echo "ABORT power ${PW}W<300 (run lock_clocks.sh)"; echo FAIL > results/campaign/.capstiming.status; exit 1; }
CLK=results/campaign/clk_capstiming_${TS}.log
nvidia-smi --query-gpu=clocks.sm --format=csv,noheader,nounits -lms 250 > "$CLK" & SENT=$!; trap 'kill $SENT 2>/dev/null' EXIT

tt(){ grep -oE "Total time: [0-9.]+s" | grep -oE "[0-9.]+" | head -1; }
med3(){ printf '%s\n' "$@" | sort -n | sed -n '2p'; }

declare -A PLY=( [cloud]=assets/models/cloud/root.primitives_pyr0.ply
                 [tornado]=assets/models/tornado/tornado_pyr0.ply
                 [explosion]=assets/models/explosion/explosion_pyr0.ply
                 [bunny]=assets/models/bunny/bunny_pyr0.ply )
declare -A NP=( [cloud]=652 [tornado]=768 [explosion]=1024 [bunny]=25600 )

render(){ # $1 exe-tag  $2 asset  $3 seed -> seconds
  cp ~/winbins/exe_$1 build/bin/Release/test_runner; cp ~/winbins/ir_$1 build/device_program.optixir
  local o; o=$(SG_PLY=${PLY[$2]} SG_ENV=white_constant SG_ALBEDO=0.9 SG_RES=512 SG_VIEW=diag \
        build/bin/Release/test_runner --scene asset_validation --spp 64 --sigma-multiplier 10 --seed "$3" 2>&1)
  grep -q "Cap overflow:" <<<"$o" && echo "  !!! OVERFLOW $1 $2 seed $3" >&2
  tt <<<"$o"
}

CSV=results/campaign/caps_timing.csv
echo "asset,N,tuned_med_s,safe512_med_s,penalty_pct,tuned_times,safe_times" > "$CSV"
for a in cloud tornado explosion bunny; do
  TU=(); SA=()
  for S in 0 1 2; do
    t=$(render "$a" "$a" "$S");        TU+=("$t"); echo "  $a tuned   seed $S: ${t}s"
    s=$(render safe512 "$a" "$S");     SA+=("$s"); echo "  $a safe512 seed $S: ${s}s"
  done
  tm=$(med3 "${TU[@]}"); sm=$(med3 "${SA[@]}")
  pct=$(python3 -c "print(f'{($sm/$tm-1)*100:.1f}')")
  echo "RESULT $a (N=${NP[$a]}): tuned=${tm}s  safe512=${sm}s  penalty=+${pct}%"
  echo "$a,${NP[$a]},$tm,$sm,$pct,\"${TU[*]}\",\"${SA[*]}\"" >> "$CSV"
done

cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir
echo "clock: $(sort -n "$CLK"|awk 'NR==1{m=$1}{a[NR]=$1}END{print "min="m" p50="a[int(NR/2)]" max="a[NR]}')"
echo "--- caps_timing.csv ---"; column -t -s, "$CSV"
echo "=== B5 caps-timing done $(date) ==="
echo DONE > results/campaign/.capstiming.status
