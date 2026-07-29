#!/usr/bin/env bash
# Task 1 (convergence ladder) + Task 2 (noise constant k) — corrected-NEE arm, meadow, 150 W.
# All renders via gabor_cloud.py + the fixed fork (results/campaign/nee_fix/GaborFixed). Idempotent
# (skips existing EXRs), cheapest-first (partial progress is usable), logs render times to a CSV.
#
# Reuse plan (ours arm is free): ours GT = cloud_conv 1024-spp seeds 1-4; ours ladder error seeds 5-8
# reuse cloud_conv 64/256/1024 + the just-rendered ours_spp16_seed{5-8}. Only the NEE arm renders here.
#
# NEE seed plan: GT = gt/ seeds 0,1 @2048 (already banked, DISJOINT from ladder-error seeds 2-5).
#   k (Task 2): seeds 0-15 @ 64 spp.  ladder error: seeds 2,3,4,5 @ {16,64,256,1024}.
set -uo pipefail
cd "$(dirname "$0")/../.."

F=results/campaign/nee_fix/GaborFixed
CLOUD_DIR=$PWD/assets/models/cloud
MEADOW_HDR=$PWD/assets/environment_maps/meadow_2_4k.hdr
OUT=results/campaign/nee_fair/ladder; mkdir -p "$OUT"
CSV="$OUT/nee_render_times.csv"; [ -f "$CSV" ] || echo "arm,spp,seed,time_s,mean" > "$CSV"
LOG="$OUT/ladder_$(date +%m%d_%H%M).log"; exec > >(tee "$LOG") 2>&1
echo "=== NEE ladder+k start $(date) (150 W) ==="

render_nee() {  # $1=spp $2=seed
  local spp=$1
  local seed=$2
  local f="$OUT/gabor_nee_meadow_spp${spp}_seed${seed}.exr"
  if [ -f "$f" ]; then echo "  spp$spp seed$seed present, skip"; return; fi
  local t0 t1 out
  t0=$(date +%s.%N)
  out=$(VOLPRIM_DIR=$PWD/$F CLOUD_DIR=$CLOUD_DIR MEADOW_HDR=$MEADOW_HDR \
        SG_ENV=meadow SG_NEE=1 SG_SPP=$spp SG_SEED=$seed SG_OUTDIR=$OUT \
        experiments/mitsuba-reference/with_pip_gabor.sh experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/gabor_cloud.py 2>&1)
  t1=$(date +%s.%N)
  local mean; mean=$(grep -oE "mean=[0-9.]+" <<<"$out" | head -1 | cut -d= -f2)
  local dt; dt=$(awk "BEGIN{printf \"%.1f\", $t1-$t0}")
  if [ -f "$f" ]; then echo "  spp$spp seed$seed OK  ${dt}s  mean=${mean:-?}"; echo "nee,$spp,$seed,$dt,${mean:-NA}" >> "$CSV";
  else echo "  !!! spp$spp seed$seed FAILED"; grep -E "Error|Traceback" <<<"$out" | head -3; fi
}

echo "--- rung 16 spp (ladder seeds 2-5) ---"
for s in 2 3 4 5; do render_nee 16 $s; done

echo "--- rung 64 spp (ladder seeds 2-5 first, then k-fill 0,1,6-15) ---"
for s in 2 3 4 5 0 1 6 7 8 9 10 11 12 13 14 15; do render_nee 64 $s; done

echo "--- rung 256 spp (ladder seeds 2-5) ---"
for s in 2 3 4 5; do render_nee 256 $s; done

echo "--- rung 1024 spp (ladder seeds 2-5) — the long pole (~1.9 h) ---"
for s in 2 3 4 5; do render_nee 1024 $s; done

echo "=== NEE ladder+k done $(date) ==="
echo "DONE" > "$OUT/.ladder.status"