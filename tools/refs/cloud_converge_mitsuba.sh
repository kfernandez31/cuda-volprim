#!/usr/bin/env bash
# Resumable multi-seed Mitsuba cloud-scatter reference (cam0). Idempotent: skips
# existing EXRs. Averaged downstream into a converged near-ground-truth M*.
set -e
cd /home/kacper/thesis
OUT=renders/cloud_converge; mkdir -p $OUT
SPP=512
for seed in 1 2 3 4 5 6 7 8; do
  f="$OUT/mitsuba_s${SPP}_seed${seed}.exr"
  if [ -f "$f" ]; then echo "seed $seed [skip]"; continue; fi
  t0=$SECONDS
  SG_ALBEDO=0.9 SG_SIGMA=7.5 SG_MAX_DEPTH=128 SG_RFILTER=box SG_CAM=0 SG_SPP=$SPP SG_SEED=$seed \
    tools/refs/with_jorge_mitsuba.sh tools/refs/.venv/bin/python tools/refs/render_cloud_prb_absorption.py >/dev/null 2>&1
  cp assets/cloud/refs_prb_scattering/0000.exr "$f"
  echo "seed $seed  $((SECONDS-t0))s"
done
echo "ALL SEEDS DONE"
