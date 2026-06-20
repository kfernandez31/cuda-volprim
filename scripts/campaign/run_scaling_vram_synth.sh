#!/usr/bin/env bash
# Fill the intermediate synthetic-grid peak VRAM points for fig:scaling's memory panel
# (currently only N=16 and N=8192 are measured). SAFE-512 binary => CONSTANT caps, only N varies;
# expectation: ~1200 MiB flat for all N (reservation dominates). Per-process poll. Cap-immune (VRAM).
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
cp ~/winbins/exe_safe512 build/bin/Release/test_runner
cp ~/winbins/ir_safe512  build/device_program.optixir
OUT=results/campaign/scaling_vram_synth.csv
echo "N,peak_mib" > "$OUT"
echo "=== scaling synth VRAM start $(date) ==="

poll_peak() {
  local pid=$1 peak=0 cur
  while kill -0 "$pid" 2>/dev/null; do
    cur=$(nvidia-smi --query-compute-apps=pid,used_memory --format=csv,noheader,nounits 2>/dev/null \
            | awk -F', *' -v p="$pid" '$1==p{print $2}')
    [ -n "${cur:-}" ] && [ "$cur" -gt "$peak" ] 2>/dev/null && peak=$cur
    sleep 0.1
  done
  echo "$peak"
}

for N in 16 256 512 1024 2048 4096 8192; do
  build/bin/Release/test_runner --scene stress_${N}_gaussians --width 512 --height 512 --spp 64 --seed 0 \
    >/tmp/scstress_$N.log 2>&1 &
  pid=$!
  peak=$(poll_peak "$pid")
  wait "$pid" 2>/dev/null
  echo "stress_$N peak=${peak} MiB"
  echo "$N,$peak" >> "$OUT"
done

cp ~/winbins/exe_stock build/bin/Release/test_runner
cp ~/winbins/ir_stock  build/device_program.optixir
echo "=== done $(date) ==="; cat "$OUT"
