#!/usr/bin/env bash
# G6 wavefront confirm: megakernel (OFF) vs wavefront (ON) at the deprecated-wavefront-phase1 tip,
# same commit/caps → the ratio is the architecture cost. One confirming point suffices (dev margin
# 100–1400×). Wavefront arm is timeout-guarded (a lower bound is still a valid confirm). Run from the
# main repo root so assets/ resolve; each binary bakes its own absolute OPTIXIR_PATH. Serialized GPU.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
exec > >(tee results/campaign/g6_wavefront.log) 2>&1
echo "=== G6 wavefront start $(date) ==="
rm -f results/campaign/.g6_wavefront.status
MEGA=wt-wavefront/build-mega/bin/Release/test_runner
WF=wt-wavefront/build-wf/bin/Release/test_runner
OUT=results/campaign/g6_wavefront; mkdir -p "$OUT"
echo "scene,arm,spp,time_s,note" > "$OUT/times.csv"
WF_TIMEOUT=240

run() { # $1=scene $2=spp ; renders mega (full) then wf (timeout-guarded)
  local scene=$1 spp=$2 o t
  echo "--- $scene @${spp}spp : megakernel ---"
  o=$(SG_ENV=meadow SG_CAM=0 $MEGA --scene "$scene" --spp "$spp" --seed 1 2>&1)
  t=$(grep -oE "Total time: [0-9.]+s" <<<"$o" | grep -oE "[0-9.]+")
  echo "$scene,mega,$spp,${t:-NA}," | tee -a "$OUT/times.csv"
  echo "--- $scene @${spp}spp : wavefront (<=${WF_TIMEOUT}s) ---"
  local start end
  start=$(date +%s.%N)
  if o=$(timeout ${WF_TIMEOUT}s env SG_ENV=meadow SG_CAM=0 $WF --scene "$scene" --spp "$spp" --seed 1 2>&1); then
    t=$(grep -oE "Total time: [0-9.]+s" <<<"$o" | grep -oE "[0-9.]+")
    echo "$scene,wf,$spp,${t:-NA},completed" | tee -a "$OUT/times.csv"
  else
    end=$(date +%s.%N)
    echo "$scene,wf,$spp,>=${WF_TIMEOUT},TIMEOUT(lower-bound)" | tee -a "$OUT/times.csv"
  fi
}

run single_gaussian_validation 4
run cloud_asset_scattering 1

echo "=== G6 wavefront done $(date) ==="
cat "$OUT/times.csv"
echo DONE > results/campaign/.g6_wavefront.status
