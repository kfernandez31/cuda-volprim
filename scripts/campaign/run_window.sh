#!/usr/bin/env bash
# Phase-B "fast timing" bucket for Piotr's max-perf window (11:30-13:30).
# Runs the cloud-only timing work that fits in 2h: RR + meadow-RIS re-anchors
# (k banked, time-only), G3 flat + studio RIS rungs (fresh k), fast-erf A/B.
# Pure timing -> REQUIRES >=300 W. Uses the stashed cloud calibrated pair.
# Headless-safe: launch under  setsid nohup bash scripts/campaign/run_window.sh ...
set -uo pipefail            # NOT -e: grep/test nonzero must not kill the run
cd "$(git rev-parse --show-toplevel)"
TS=$(date +%H%M)
MARK=results/campaign/.window_${TS}
exec > >(tee results/campaign/window_${TS}.log) 2>&1
echo "=== WINDOW RUN start $(date) ==="

# ---- precondition: power must be lifted, else timings are garbage (rule R1) ----
PW=$(nvidia-smi --query-gpu=power.limit --format=csv,noheader,nounits | cut -d. -f1)
if [ "${PW:-0}" -lt 300 ]; then
  echo "ABORT: power ${PW}W < 300W — refusing to record invalid timings. Bump to 350W first."
  echo "FAIL" > ${MARK}.status; exit 1
fi
echo "power=${PW}W  clocks=$(nvidia-smi --query-gpu=clocks.sm,clocks.mem --format=csv,noheader)"

# ---- install the cloud calibrated pair into build/ (rule R3: exe + optixir together) ----
for f in exe_cloud ir_cloud; do
  [ -f ~/winbins/$f ] || { echo "ABORT: ~/winbins/$f missing (run the pre-window builds first)"; echo FAIL > ${MARK}.status; exit 1; }
done
cp ~/winbins/exe_cloud build/bin/Release/test_runner
cp ~/winbins/ir_cloud  build/device_program.optixir
BIN=build/bin/Release/test_runner

# ---- clock sentinel for the whole window (rule: report min/p50/max) ----
nvidia-smi --query-gpu=clocks.sm --format=csv,noheader,nounits -lms 200 > results/campaign/clk_window_${TS}.log &
SENT=$!; trap 'kill $SENT 2>/dev/null' EXIT

render_time() {  # $1=extra-flags-string ; echoes Total time seconds ; uses globals SG_ENV/SG_CAM
  SG_CAM=0 $BIN --scene cloud_asset_scattering --spp 64 --seed "$SEED" $1 2>&1 \
    | grep -oE "Total time: [0-9.]+s" | grep -oE "[0-9.]+"
}

# =====================================================================
# 1. RR-depth re-anchor (TIME ONLY — k banked, rule R8). meadow. 5 interleaved rounds.
# =====================================================================
echo "--- [1/5] RR re-anchor (meadow, 5 rounds x depths{5,6,8,10,12,16}) ---"
RRLOG=results/campaign/rr_rerun_${TS}.log; : > $RRLOG
for r in 1 2 3 4 5; do for d in 5 6 8 10 12 16; do
  SEED=1; t=$(SG_ENV=meadow render_time "--rr-depth $d")
  echo "round=$r depth=$d t=$t" | tee -a $RRLOG
done; done

# =====================================================================
# 2. meadow RIS re-anchor (TIME ONLY — k banked). 5 interleaved rounds, 7 arms.
# =====================================================================
echo "--- [2/5] meadow RIS re-anchor (5 rounds x {mis,1,2,4,6,8,12}) ---"
RISMLOG=results/campaign/ris_meadow_rerun_${TS}.log; : > $RISMLOG
for r in 1 2 3 4 5; do for arm in mis 1 2 4 6 8 12; do
  [ "$arm" = mis ] && F="" || F="--ris --ris-candidates $arm"
  SEED=1; t=$(SG_ENV=meadow render_time "$F")
  echo "round=$r arm=$arm t=$t" | tee -a $RISMLOG
done; done

# =====================================================================
# 3. G3 FLAT rung (FRESH k). white_constant. 16 seeds x 7 arms, EXRs kept.
# =====================================================================
echo "--- [3/5] G3 flat rung (white_constant, 16 seeds x 7 arms) ---"
OUT=results/campaign/ris_seeds_flat; mkdir -p $OUT
grep -qxF "results/campaign/ris_seeds_flat/" .gitignore || echo "results/campaign/ris_seeds_flat/" >> .gitignore
echo "arm,seed,time_s" > $OUT/times.csv
# env-leak guard: first render must load white_constant, NOT meadow/studio
ENVLINE=$(unset SG_ENV; SG_CAM=0 $BIN --scene cloud_asset_scattering --spp 1 --seed 1 2>&1 | grep -iE "environment map|white_constant|meadow|studio" | head -1)
echo "  flat env check: $ENVLINE"
case "$ENVLINE" in *meadow*|*studio*) echo "ABORT: flat rung leaked a non-constant env"; echo FAIL > ${MARK}.status; exit 1;; esac
for s in $(seq 1 16); do for arm in mis 1 2 4 6 8 12; do
  [ "$arm" = mis ] && F="" || F="--ris --ris-candidates $arm"
  t=$(unset SG_ENV; SG_CAM=0 $BIN --scene cloud_asset_scattering --spp 64 --seed $s $F 2>&1 | grep -oE "Total time: [0-9.]+s" | grep -oE "[0-9.]+")
  cp test_results/cloud_asset_scattering/0000.exr $OUT/${arm}_s${s}.exr
  echo "$arm,$s,$t" >> $OUT/times.csv
done; echo "  flat seed $s done"; done

# =====================================================================
# 4. G3 STUDIO rung (FRESH k). ferndale_studio_01. 16 seeds x 7 arms, EXRs kept.
# =====================================================================
echo "--- [4/5] G3 studio rung (SG_ENV=studio, 16 seeds x 7 arms) ---"
OUT=results/campaign/ris_seeds_studio; mkdir -p $OUT
grep -qxF "results/campaign/ris_seeds_studio/" .gitignore || echo "results/campaign/ris_seeds_studio/" >> .gitignore
echo "arm,seed,time_s" > $OUT/times.csv
ENVLINE=$(SG_ENV=studio SG_CAM=0 $BIN --scene cloud_asset_scattering --spp 1 --seed 1 2>&1 | grep -iE "environment map|ferndale|studio" | head -1)
echo "  studio env check: $ENVLINE"
case "$ENVLINE" in *ferndale*|*studio*) : ;; *) echo "ABORT: studio rung did not load ferndale"; echo FAIL > ${MARK}.status; exit 1;; esac
for s in $(seq 1 16); do for arm in mis 1 2 4 6 8 12; do
  [ "$arm" = mis ] && F="" || F="--ris --ris-candidates $arm"
  t=$(SG_ENV=studio SG_CAM=0 $BIN --scene cloud_asset_scattering --spp 64 --seed $s $F 2>&1 | grep -oE "Total time: [0-9.]+s" | grep -oE "[0-9.]+")
  cp test_results/cloud_asset_scattering/0000.exr $OUT/${arm}_s${s}.exr
  echo "$arm,$s,$t" >> $OUT/times.csv
done; echo "  studio seed $s done"; done

# =====================================================================
# 5. fast-erf A/B (marginal-at-final, interleaved). Needs stashed fast-erf pair.
# =====================================================================
if [ -f ~/winbins/exe_ferf ] && [ -f ~/winbins/ir_ferf ]; then
  echo "--- [5/5] fast-erf A/B (cloud-meadow 64spp, 5 interleaved rounds) ---"
  FERFLOG=results/campaign/ferf_ab_${TS}.log; : > $FERFLOG
  for r in 1 2 3 4 5; do
    cp ~/winbins/exe_cloud build/bin/Release/test_runner; cp ~/winbins/ir_cloud build/device_program.optixir
    SEED=1; ts=$(SG_ENV=meadow render_time ""); echo "round=$r arm=stock t=$ts" | tee -a $FERFLOG
    cp ~/winbins/exe_ferf build/bin/Release/test_runner; cp ~/winbins/ir_ferf build/device_program.optixir
    SEED=1; tf=$(SG_ENV=meadow render_time ""); echo "round=$r arm=ferf  t=$tf" | tee -a $FERFLOG
  done
  cp ~/winbins/exe_cloud build/bin/Release/test_runner; cp ~/winbins/ir_cloud build/device_program.optixir
else
  echo "--- [5/5] fast-erf SKIPPED (no ~/winbins/exe_ferf) ---"
fi

echo "=== WINDOW RUN done $(date) ==="
echo "clock sentinel: $(sort -n results/campaign/clk_window_${TS}.log | awk 'NR==1{min=$1} {a[NR]=$1} END{print "min="min" p50="a[int(NR/2)]" max="a[NR]}')"
echo "DONE" > ${MARK}.status
