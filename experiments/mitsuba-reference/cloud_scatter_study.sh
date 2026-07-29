#!/usr/bin/env bash
# Resumable cloud-scattering spp-ladder study (cam0, sigma=7.5 albedo=0.9 depth=128 box).
# Idempotent: skips any render whose EXR already exists, so it survives a reboot —
# just re-run this script and it continues from where it stopped.
set -e
cd /home/kacper/thesis
OUT=renders/cloud_scatter_study; mkdir -p $OUT
SPPS="64 128 256 512 1024"
echo "===== CUDA renders ====="
for s in $SPPS; do
  if [ -f "$OUT/cuda_spp${s}.exr" ]; then echo "CUDA spp=$s  [skip, exists]"; continue; fi
  t0=$SECONDS
  SG_CAM=0 SG_ALBEDO=0.9 ./build/bin/Release/test_runner --scene cloud_asset_scattering \
    --sigma-multiplier 7.5 --spp $s --output-dir renders/cloud_scatter >/dev/null 2>&1
  dt=$((SECONDS-t0))
  cp renders/cloud_scatter/cloud_asset_scattering/0000.exr $OUT/cuda_spp${s}.exr
  echo "CUDA spp=$s  ${dt}s"; echo "cuda $s $dt" >> $OUT/timing.txt
done
echo "===== Mitsuba renders ====="
for s in $SPPS; do
  if [ -f "$OUT/mitsuba_spp${s}.exr" ]; then echo "Mitsuba spp=$s  [skip, exists]"; continue; fi
  t0=$SECONDS
  SG_ALBEDO=0.9 SG_SIGMA=7.5 SG_MAX_DEPTH=128 SG_RFILTER=box SG_CAM=0 SG_SPP=$s \
    experiments/mitsuba-reference/with_jorge_mitsuba.sh experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/render_cloud_prb_absorption.py >/dev/null 2>&1
  dt=$((SECONDS-t0))
  cp assets/models/cloud/refs_prb_scattering/0000.exr $OUT/mitsuba_spp${s}.exr
  echo "Mitsuba spp=$s  ${dt}s"; echo "mitsuba $s $dt" >> $OUT/timing.txt
done
echo "===== ALL RENDERS DONE ====="
