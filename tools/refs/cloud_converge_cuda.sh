#!/usr/bin/env bash
# Resumable multi-seed CUDA cloud-scatter renders (cam0) for the converged CUDA* image.
# Idempotent: skips existing EXRs. 16 seeds @512spp -> CUDA* noise ~0.0045.
set -e
cd /home/kacper/thesis
OUT=renders/cloud_converge; mkdir -p $OUT
SPP=512
for seed in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16; do
  f="$OUT/cuda_s${SPP}_seed${seed}.exr"
  if [ -f "$f" ]; then echo "cuda seed $seed [skip]"; continue; fi
  t0=$SECONDS
  SG_CAM=0 SG_ALBEDO=0.9 ./build/bin/Release/test_runner --scene cloud_asset_scattering \
    --sigma-multiplier 7.5 --spp $SPP --seed $seed --output-dir renders/cloud_scatter >/dev/null 2>&1
  cp renders/cloud_scatter/cloud_asset_scattering/0000.exr "$f"
  echo "cuda seed $seed  $((SECONDS-t0))s"
done
echo "ALL CUDA SEEDS DONE"
