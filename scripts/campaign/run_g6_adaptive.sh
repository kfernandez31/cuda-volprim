#!/usr/bin/env bash
# G6 adaptive confirm: adaptive-ON vs uniform (stock) at equal spp on cloud-meadow; RMSE vs the banked
# 1024-spp GT confirms equal quality, so the time ratio IS the efficiency loss. Serialized GPU work
# (no concurrent builds). 150 W (both arms same power → ratio robust; margin is the point).
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
exec > >(tee results/campaign/g6_adaptive.log) 2>&1
echo "=== G6 adaptive start $(date) ==="
rm -f results/campaign/.g6_adaptive.status
OUT=results/campaign/g6_adaptive; mkdir -p "$OUT"
echo "arm,seed,time_s,overflow" > "$OUT/times.csv"
SEEDS="0 1 2"
for S in $SEEDS; do
  for A in adapt stock; do
    cp ~/winbins/exe_$A build/bin/Release/test_runner; cp ~/winbins/ir_$A build/device_program.optixir
    o=$(SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering \
          --sigma-multiplier 7.5 --spp 64 --seed "$S" 2>&1)
    ovf=0; grep -q "Cap overflow:" <<<"$o" && ovf=1
    t=$(grep -oE "Total time: [0-9.]+s" <<<"$o" | grep -oE "[0-9.]+")
    [ -f test_results/cloud_asset_scattering/0000.exr ] && cp test_results/cloud_asset_scattering/0000.exr "$OUT/${A}_seed${S}.exr"
    echo "$A,$S,${t:-NA},$ovf" | tee -a "$OUT/times.csv"
  done
done
cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir
echo "=== G6 adaptive done $(date) ==="
echo DONE > results/campaign/.g6_adaptive.status
