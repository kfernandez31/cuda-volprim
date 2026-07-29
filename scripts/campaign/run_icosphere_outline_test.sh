#!/usr/bin/env bash
# fig 5.2 cloud-outline diagnosis: does rendering OURS with the tessellated icosphere shell change the
# silhouette outline vs Mitsuba (exact 'ellipsoids')? Self-consistent triple at matched config. Also
# renders ours-exact & Mitsuba at 3 spp to test whether the edge residual is SYSTEMATIC (spp-invariant
# => cross-implementation bias) or NOISE (shrinks with spp). Absorption (albedo 0), white-constant, box.
set -uo pipefail; cd "$(git rev-parse --show-toplevel)"
OUT=results/campaign/ladder_icos; mkdir -p "$OUT"
grep -qxF "results/campaign/ladder_icos/" .gitignore || echo "results/campaign/ladder_icos/" >> .gitignore
LOG="$OUT/run.log"; exec > >(tee "$LOG") 2>&1
echo "=== icosphere outline test start $(date) ==="
SIG=7.5
render_ours(){ SG_ENV=white_constant SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_validation \
  --sigma-multiplier $SIG --spp "$1" --seed 1 >/dev/null 2>&1; cp test_results/cloud_asset_validation/0000.exr "$2"; }
render_mits(){ SG_ALBEDO=0 SG_ENV=white_constant SG_SIGMA=$SIG SG_SPP="$1" SG_SEED=1 SG_MAX_DEPTH=128 SG_RFILTER=box \
  experiments/mitsuba-reference/with_jorge_mitsuba.sh experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/render_cloud_prb_absorption.py >/dev/null 2>&1
  cp assets/models/cloud/refs_prb_absorption/0000.exr "$2"; }

# ours-exact (current default build) at 64/256/1024
for s in 64 256 1024; do render_ours $s "$OUT/ours_exact_spp$s.exr"; echo "ours-exact spp$s done"; done
# Mitsuba-exact at 64/256/1024
for s in 64 256 1024; do render_mits $s "$OUT/mits_spp$s.exr"; echo "mits spp$s done"; done

# ours-ICOSPHERE (tessellated GAS) at 256 — reconfigure, build, render, restore
cmake -B build -DTHESIS_ICOSPHERE=ON -DTHESIS_ICOSPHERE_N=3 >/dev/null 2>&1
touch device/device_program.cu; cmake --build build --target test_runner -j"$(nproc)" >/dev/null 2>&1 && echo "icosphere built"
render_ours 256 "$OUT/ours_icosphere_spp256.exr"; echo "ours-icosphere spp256 done"
cmake -B build -DTHESIS_ICOSPHERE=OFF >/dev/null 2>&1
touch device/device_program.cu; cmake --build build --target test_runner -j"$(nproc)" >/dev/null 2>&1 && echo "default build restored"
echo "=== done $(date) ==="
