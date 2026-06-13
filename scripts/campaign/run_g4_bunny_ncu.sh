#!/usr/bin/env bash
# G4 bunny ncu profile — cap-immune (ncu base-locks clocks). Bunny calibrated pair (80/528),
# asset_validation, SG_RES=256, meadow, albedo 0.9, render megakernel (regex:optixLaunch, 1 launch).
# Sections for the bottleneck story + a metrics pass for the roofline point. Headless-safe.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
TS=$(date +%H%M)
exec > >(tee results/campaign/g4_bunny_ncu_${TS}.log) 2>&1
echo "=== G4 bunny ncu start $(date) ==="
rm -f results/campaign/.g4.status
NCU=$(command -v ncu || echo /usr/local/cuda/bin/ncu)
cp ~/winbins/exe_bunny build/bin/Release/test_runner; cp ~/winbins/ir_bunny build/device_program.optixir

SEC=results/campaign/g4_bunny_sections_${TS}.txt
echo "--- sections (Occupancy/SpeedOfLight/SchedulerStats/WarpStateStats) ---"
SG_PLY=assets/models/bunny/bunny_pyr0.ply SG_ENV=meadow SG_ALBEDO=0.9 SG_RES=256 SG_VIEW=diag \
  "$NCU" --kernel-name "regex:optixLaunch" --launch-count 1 \
  --section Occupancy --section SpeedOfLight --section SchedulerStats --section WarpStateStats \
  build/bin/Release/test_runner --scene asset_validation --spp 4 > "$SEC" 2>&1 || true
echo "rows captured: $(grep -c '.' "$SEC")"
grep -iE "Achieved Occupancy|Registers Per Thread|Compute \(SM\)|Memory Throughput|DRAM Throughput|Eligible Warps|Issued Warp|No Eligible|Avg. Active Threads|Warp Cycles Per" "$SEC" | head -30

ROOF=results/campaign/g4_bunny_roofline_${TS}.csv
echo "--- roofline metrics ---"
SG_PLY=assets/models/bunny/bunny_pyr0.ply SG_ENV=meadow SG_ALBEDO=0.9 SG_RES=256 SG_VIEW=diag \
  "$NCU" --kernel-name "regex:optixLaunch" --launch-count 1 --csv \
  --metrics gpu__time_duration.sum,dram__bytes.sum,sm__inst_executed_pipe_fma.sum,smsp__thread_inst_executed_per_inst_executed.ratio,sm__throughput.avg.pct_of_peak_sustained_elapsed,gpu__dram_throughput.avg.pct_of_peak_sustained_elapsed \
  build/bin/Release/test_runner --scene asset_validation --spp 4 > "$ROOF" 2>&1 || true
tail -10 "$ROOF"

cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir
echo "=== G4 bunny ncu done $(date) ==="
echo DONE > results/campaign/.g4.status
