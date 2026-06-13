#!/usr/bin/env bash
# Phase-B "fast timing" bucket for the max-perf window. Cloud-only timing that fits in ~40min+buffer:
# RR + meadow-RIS re-anchors (k banked, time-only), G3 flat + studio RIS rungs (fresh k).
# Pure timing -> REQUIRES >=300 W. Uses the stashed cloud calibrated pair (64/96).
# Checks EVERY render for "Cap overflow" (Kacper's directive) and tallies them into the status.
# Headless:  setsid nohup bash scripts/campaign/run_window.sh >/dev/null 2>&1 </dev/null &
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
TS=$(date +%H%M)
MARK=results/campaign/.window_${TS}
exec > >(tee results/campaign/window_${TS}.log) 2>&1
echo "=== WINDOW RUN start $(date) ==="

# precondition: power lifted (rule R1) — refuse to record invalid timings
PW=$(nvidia-smi --query-gpu=power.limit --format=csv,noheader,nounits | cut -d. -f1)
if [ "${PW:-0}" -lt 300 ]; then
  echo "ABORT: power ${PW}W < 300W. Bump to 350W first (sudo nvidia-smi -pl 350 -lgc 1800,1800)."
  echo FAIL > ${MARK}.status; exit 1
fi
echo "power=${PW}W  clocks=$(nvidia-smi --query-gpu=clocks.sm,clocks.mem --format=csv,noheader)"

# install cloud calibrated pair (rule R3: exe + optixir together)
for f in exe_cloud ir_cloud; do
  [ -f ~/winbins/$f ] || { echo "ABORT: ~/winbins/$f missing"; echo FAIL > ${MARK}.status; exit 1; }
done
cp ~/winbins/exe_cloud build/bin/Release/test_runner
cp ~/winbins/ir_cloud  build/device_program.optixir
BIN=build/bin/Release/test_runner

# clock sentinel for the whole run
nvidia-smi --query-gpu=clocks.sm --format=csv,noheader,nounits -lms 200 > results/campaign/clk_window_${TS}.log &
SENT=$!; trap 'kill $SENT 2>/dev/null' EXIT

declare -i OVERFLOW_HITS=0
# run1: render cloud_asset_scattering at current SG_ENV/SEED + $1 flags.
#       sets REPLY_T (seconds); checks "Cap overflow" on EVERY render; optional $2 = EXR dest.
run1() {
  local out; out=$(SG_CAM=0 $BIN --scene cloud_asset_scattering --spp 64 --seed "$SEED" $1 2>&1)
  if grep -q "Cap overflow:" <<<"$out"; then
    OVERFLOW_HITS+=1
    echo "!!! CAP OVERFLOW seed=$SEED flags='$1' env='${SG_ENV:-white}': $(grep 'Cap overflow:' <<<"$out" | head -1)" \
      | tee -a results/campaign/OVERFLOW_${TS}.log
  fi
  REPLY_T=$(grep -oE "Total time: [0-9.]+s" <<<"$out" | grep -oE "[0-9.]+")
  [ -n "${2:-}" ] && cp test_results/cloud_asset_scattering/0000.exr "$2"
}

# 1. RR re-anchor (TIME ONLY — k banked, R8). meadow. 5 interleaved rounds.
echo "--- [1/4] RR re-anchor (meadow, depths{5,6,8,10,12,16}) ---"
RRLOG=results/campaign/rr_rerun_${TS}.log; : > $RRLOG
export SG_ENV=meadow; SEED=1
for r in 1 2 3 4 5; do for d in 5 6 8 10 12 16; do
  run1 "--rr-depth $d"; echo "round=$r depth=$d t=$REPLY_T" | tee -a $RRLOG
done; done

# 2. meadow RIS re-anchor (TIME ONLY). 5 interleaved rounds, 7 arms.
echo "--- [2/4] meadow RIS re-anchor ({mis,1,2,4,6,8,12}) ---"
RISMLOG=results/campaign/ris_meadow_rerun_${TS}.log; : > $RISMLOG
for r in 1 2 3 4 5; do for arm in mis 1 2 4 6 8 12; do
  [ "$arm" = mis ] && F="" || F="--ris --ris-candidates $arm"
  run1 "$F"; echo "round=$r arm=$arm t=$REPLY_T" | tee -a $RISMLOG
done; done
unset SG_ENV

# 3. G3 FLAT rung (FRESH k). white_constant. 16 seeds x 7 arms, EXRs kept.
echo "--- [3/4] G3 flat rung (white_constant) ---"
OUT=results/campaign/ris_seeds_flat; mkdir -p $OUT
grep -qxF "results/campaign/ris_seeds_flat/" .gitignore || echo "results/campaign/ris_seeds_flat/" >> .gitignore
echo "arm,seed,time_s" > $OUT/times.csv
ENVLINE=$(SG_CAM=0 $BIN --scene cloud_asset_scattering --spp 1 --seed 1 2>&1 | grep -iE "environment map" | head -1)
echo "  flat env: $ENVLINE"
case "$ENVLINE" in *meadow*|*ferndale*) echo "ABORT: flat rung leaked a non-constant env"; echo FAIL > ${MARK}.status; exit 1;; esac
for s in $(seq 1 16); do SEED=$s; for arm in mis 1 2 4 6 8 12; do
  [ "$arm" = mis ] && F="" || F="--ris --ris-candidates $arm"
  run1 "$F" "$OUT/${arm}_s${s}.exr"; echo "$arm,$s,$REPLY_T" >> $OUT/times.csv
done; echo "  flat seed $s done"; done

# 4. G3 STUDIO rung (FRESH k). ferndale_studio_01. 16 seeds x 7 arms, EXRs kept.
echo "--- [4/4] G3 studio rung (SG_ENV=studio) ---"
OUT=results/campaign/ris_seeds_studio; mkdir -p $OUT
grep -qxF "results/campaign/ris_seeds_studio/" .gitignore || echo "results/campaign/ris_seeds_studio/" >> .gitignore
echo "arm,seed,time_s" > $OUT/times.csv
export SG_ENV=studio
ENVLINE=$(SG_CAM=0 $BIN --scene cloud_asset_scattering --spp 1 --seed 1 2>&1 | grep -iE "environment map" | head -1)
echo "  studio env: $ENVLINE"
case "$ENVLINE" in *ferndale*|*studio*) : ;; *) echo "ABORT: studio rung did not load ferndale"; echo FAIL > ${MARK}.status; exit 1;; esac
for s in $(seq 1 16); do SEED=$s; for arm in mis 1 2 4 6 8 12; do
  [ "$arm" = mis ] && F="" || F="--ris --ris-candidates $arm"
  run1 "$F" "$OUT/${arm}_s${s}.exr"; echo "$arm,$s,$REPLY_T" >> $OUT/times.csv
done; echo "  studio seed $s done"; done
unset SG_ENV

echo "=== WINDOW RUN done $(date) ==="
echo "clock sentinel: $(sort -n results/campaign/clk_window_${TS}.log | awk 'NR==1{min=$1}{a[NR]=$1}END{print "min="min" p50="a[int(NR/2)]" max="a[NR]}')"
echo "CAP OVERFLOWS THIS RUN: $OVERFLOW_HITS  (expect 0 — cloud demand 85/45 « caps 96/64)"
[ "$OVERFLOW_HITS" -eq 0 ] && echo "DONE" > ${MARK}.status || echo "DONE_WITH_OVERFLOWS" > ${MARK}.status
