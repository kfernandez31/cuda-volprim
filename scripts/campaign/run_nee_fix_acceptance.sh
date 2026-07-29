#!/usr/bin/env bash
# Acceptance for the volprim NEE fix stack (FIX1 depth-0 MIS, FIX2 kernel B-sign, FIX3 march segment,
# FIX4 support clip) in the fork results/campaign/nee_fix/GaborFixed.
# PASS: every NEE furnace row |dev| <~ 0.02pp (4096 spp), every analog row exactly 1.000000;
# A5 cloud: NEE must agree with ITS OWN analog (remaining analog<->truth gap = defect #2, separate).
set -uo pipefail; cd "$(git rev-parse --show-toplevel)"
F=results/campaign/nee_fix/GaborFixed; LOG=results/campaign/nee_fix/acceptance_$(date +%m%d_%H%M).log
exec > >(tee "$LOG") 2>&1
run(){ env "$@" experiments/mitsuba-reference/with_pip_gabor.sh experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/gabor_furnace.py 2>&1 | grep RESULT; }

echo "== A1 furnace matrix (NEE spp4096 + analog spp1024), sigma=6, extent=3 =="
for cfg in "1 0" "2 0" "16 0" "2 1" "2 2" "2 3" "2 4" "4 2" "8 2"; do set -- $cfg
  run VOLPRIM_DIR=$PWD/$F SG_NPRIM=$1 SG_SPACING=$2 SG_NEE=1 SG_ALBEDO=1.0 SG_SIGMA=6 SG_MAX_DEPTH=-1 SG_SPP=4096 SG_SEED=0 SG_ARM="nee_N$1_d$2"
  run VOLPRIM_DIR=$PWD/$F SG_NPRIM=$1 SG_SPACING=$2 SG_NEE=0 SG_ALBEDO=1.0 SG_SIGMA=6 SG_MAX_DEPTH=-1 SG_SPP=1024 SG_SEED=0 SG_ARM="analog_N$1_d$2"
done

echo "== A2 sigma sweep at N=2 d=3 + aniso + thin rung =="
for s in 1 6 12 30; do
  run VOLPRIM_DIR=$PWD/$F SG_NPRIM=2 SG_SPACING=3 SG_NEE=1 SG_ALBEDO=1.0 SG_SIGMA=$s SG_MAX_DEPTH=-1 SG_SPP=4096 SG_SEED=0 SG_ARM="nee_sweep_s$s"
done
run VOLPRIM_DIR=$PWD/$F SG_TRANSFORMED=1 SG_NEE=1 SG_ALBEDO=1.0 SG_SIGMA=6 SG_MAX_DEPTH=-1 SG_SPP=4096 SG_SEED=0 SG_ARM="nee_aniso"
run VOLPRIM_DIR=$PWD/$F SG_NEE=1 SG_ALBEDO=1.0 SG_SIGMA=60.55 SG_MAX_DEPTH=-1 SG_SPP=4096 SG_SEED=0 SG_ARM="nee_thin_60p55"

echo "== A3 white-envmap emitter path (2D-CDF sampling instead of constant) =="
run VOLPRIM_DIR=$PWD/$F SG_ENVMAP_FILE=$PWD/results/campaign/furnace_supervisor/white_env.exr SG_NPRIM=4 SG_SPACING=2 SG_NEE=1 SG_ALBEDO=1.0 SG_SIGMA=6 SG_MAX_DEPTH=-1 SG_SPP=4096 SG_SEED=0 SG_ARM="nee_whiteenv"

echo "== A4 solver/backend spot checks =="
run VOLPRIM_DIR=$PWD/$F SG_SOLVER=newton SG_NPRIM=4 SG_SPACING=2 SG_NEE=1 SG_ALBEDO=1.0 SG_SIGMA=6 SG_MAX_DEPTH=-1 SG_SPP=4096 SG_SEED=0 SG_ARM="nee_newton"
run VOLPRIM_DIR=$PWD/$F SG_VARIANT=llvm_ad_rgb SG_NPRIM=4 SG_SPACING=2 SG_NEE=1 SG_ALBEDO=1.0 SG_SIGMA=6 SG_MAX_DEPTH=-1 SG_SPP=256 SG_SEED=0 SG_ARM="nee_llvm"

echo "== A5 cloud: NEE vs its own analog (defect #2 = analog<->truth gap, stays open) =="
export CLOUD_DIR=$PWD/assets/models/cloud MEADOW_HDR=$PWD/assets/environment_maps/meadow_2_4k.hdr
for env in white_constant meadow; do for nee in 1 0; do
  VOLPRIM_DIR=$PWD/$F SG_ENV=$env SG_NEE=$nee SG_SPP=256 SG_SEED=0 \
    experiments/mitsuba-reference/with_pip_gabor.sh experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/gabor_cloud.py 2>&1 | grep RESULT
done; done
echo "== acceptance done =="
