#!/usr/bin/env bash
# G4 suite ncu — roofline points for tornado + explosion (to join cloud + bunny).
# Same recipe as run_g4_bunny_ncu.sh: asset_validation, SG_RES=256, meadow, albedo 0.9, SG_VIEW=diag,
# per-asset tuned binary, render megakernel (regex:optixLaunch, 1 launch), --metrics for the roofline.
# ncu base-locks clocks itself -> cap-immune. Headless-safe.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
TS=$(date +%H%M)
NCU=$(command -v ncu || echo /usr/local/cuda/bin/ncu)
MET=gpu__time_duration.sum,dram__bytes.sum,sm__inst_executed_pipe_fma.sum,smsp__thread_inst_executed_per_inst_executed.ratio,sm__throughput.avg.pct_of_peak_sustained_elapsed,gpu__dram_throughput.avg.pct_of_peak_sustained_elapsed

run_one() {
  local asset="$1" ply="$2"
  echo "=== $asset ncu start $(date +%H:%M:%S) ==="
  cp ~/winbins/exe_${asset} build/bin/Release/test_runner
  cp ~/winbins/ir_${asset}  build/device_program.optixir
  local out="results/campaign/g4_${asset}_roofline_${TS}.csv"
  SG_PLY="$ply" SG_ENV=meadow SG_ALBEDO=0.9 SG_RES=256 SG_VIEW=diag \
    "$NCU" --kernel-name "regex:optixLaunch" --launch-count 1 --csv --metrics "$MET" \
    build/bin/Release/test_runner --scene asset_validation --spp 4 > "$out" 2>&1
  echo "wrote $out"
  grep -iE 'ERR_NVGPUCTRPERM|not permitted|==ERROR==' "$out" && echo "!! PERMISSION/ERROR for $asset" || true
  tail -8 "$out"
}

run_one tornado   assets/models/tornado/tornado_pyr0.ply
run_one explosion assets/models/explosion/explosion_pyr0.ply

cp ~/winbins/exe_stock build/bin/Release/test_runner
cp ~/winbins/ir_stock  build/device_program.optixir
echo "=== restored stock binary; done $(date +%H:%M:%S) ==="
