#!/usr/bin/env bash
# Task 3 — equal-quality TIMING panel, ours-MIS vs corrected-NEE, meadow, 256 spp. REQUIRES locked 350 W.
# Protocol (Fable-confirmed): 1 warmup + 5 timed repeats per arm, BOTH inside the same locked-clock
# window; nvidia-smi CLOCK,POWER snapshot before and after; report t/spp with repeat spread.
# ours "Total time" (matches banked g1 convention) vs NEE steady render time (gabor_timing.py).
#
#   DRY_RUN=1 bash scripts/campaign/run_nee_fair_timing.sh   # validate mechanics at any power (NOT valid timing)
#   bash scripts/campaign/run_nee_fair_timing.sh             # real run: aborts unless power>=300W
set -uo pipefail
cd "$(dirname "$0")/../.."

F=results/campaign/nee_fix/GaborFixed
CLOUD_DIR=$PWD/assets/models/cloud
MEADOW_HDR=$PWD/assets/environment_maps/meadow_2_4k.hdr
OUT=results/campaign/nee_fair/timing; mkdir -p "$OUT"
SPP=${SPP:-256}; REPEATS=${REPEATS:-5}
DRY_RUN=${DRY_RUN:-0}
BINBAK=.binbak; mkdir -p "$BINBAK"

# --- power gate ---
PW=$(nvidia-smi --query-gpu=power.limit --format=csv,noheader,nounits | cut -d. -f1)
echo "=== GPU power limit: ${PW} W ==="
if [ "$DRY_RUN" != "1" ] && [ "${PW:-0}" -lt 300 ]; then
  echo "ABORT: power ${PW}W < 300W. Lock first: bash scripts/campaign/lock_clocks.sh  (then re-run)."
  echo "  (or DRY_RUN=1 to validate the harness mechanics only — those times are NOT reportable.)"
  exit 1
fi
[ "$DRY_RUN" = "1" ] && echo "*** DRY_RUN: mechanics only; times are NOT valid (not 350W/locked) ***"

TS=$(date +%m%d_%H%M)
CLKLOG="$OUT/clocks_${TS}.log"
snap(){ echo "===== nvidia-smi CLOCK,POWER ($1) $(date) ====="; nvidia-smi -q -d CLOCK,POWER 2>/dev/null \
        | grep -E "Power Draw|Power Limit|Graphics|SM |Memory " | head -20; }
# Guard against gpu-power-guard.sh reverting to 150W mid-run (fires when 'prybicki' logs out). Abort
# LOUDLY rather than silently record a 150W-contaminated number. Called before + after each arm.
check_power(){
  local pw; pw=$(nvidia-smi --query-gpu=power.limit --format=csv,noheader,nounits | cut -d. -f1)
  if [ "$DRY_RUN" != "1" ] && [ "${pw:-0}" -lt 300 ]; then
    echo "!!! ABORT ($1): power dropped to ${pw}W mid-run (gpu-power-guard reverted it -- prybicki logged out?)."
    echo "!!! This run is CONTAMINATED and DISCARDED. Re-lock (stay logged in) and re-run."
    rm -f "$CSV"; exit 2
  fi
}
snap before | tee "$CLKLOG"

CSV="$OUT/timing_${TS}.csv"; echo "arm,repeat,render_s" > "$CSV"

# --- back up current binary, restore on exit ---
cp build/bin/Release/test_runner "$BINBAK/tr_pretiming" 2>/dev/null || true
cp build/device_program.optixir "$BINBAK/ir_pretiming" 2>/dev/null || true
restore(){ cp "$BINBAK/tr_pretiming" build/bin/Release/test_runner 2>/dev/null || true
           cp "$BINBAK/ir_pretiming" build/device_program.optixir 2>/dev/null || true
           echo "[restored pre-timing binary]"; }
trap restore EXIT

# ================= OURS (calibrated exe_cloud, Total time) =================
check_power "before ours"
cp ~/winbins/exe_cloud build/bin/Release/test_runner
cp ~/winbins/ir_cloud  build/device_program.optixir
echo "--- ours warmup ---"
SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering \
  --sigma-multiplier 7.5 --spp "$SPP" --seed 0 >/dev/null 2>&1
echo "--- ours timed x$REPEATS @${SPP}spp ---"
for r in $(seq 1 "$REPEATS"); do
  o=$(SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering \
        --sigma-multiplier 7.5 --spp "$SPP" --seed 0 2>&1)
  t=$(grep -oE "Total time: [0-9.]+s" <<<"$o" | grep -oE "[0-9.]+")
  echo "  ours repeat $r: ${t}s"; echo "ours,$r,${t:-NA}" >> "$CSV"
done

# ================= corrected-NEE (fork, steady render time) =================
check_power "after ours / before nee"
echo "--- NEE timed (1 warmup + $REPEATS timed) @${SPP}spp ---"
VOLPRIM_DIR=$PWD/$F CLOUD_DIR=$CLOUD_DIR MEADOW_HDR=$MEADOW_HDR \
  SG_SPP="$SPP" SG_SEED=0 SG_REPEATS="$REPEATS" \
  experiments/mitsuba-reference/with_pip_gabor.sh experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/gabor_timing.py 2>&1 \
  | tee "$OUT/nee_timing_${TS}.log" | grep -E "TIMING_S|gabor_timing" | while read -r line; do
      if [[ "$line" == TIMING_S* ]]; then
        t=${line#TIMING_S }; echo "  nee repeat: ${t}s"; echo "nee,-,${t}" >> "$CSV"
      fi
    done

check_power "after nee"
snap after | tee -a "$CLKLOG"
echo "=== raw times -> $CSV ==="; cat "$CSV"
echo "=== analysis ==="
experiments/mitsuba-reference/with_pip_gabor.sh experiments/mitsuba-reference/.venv/bin/python scripts/plots/nee_fair_timing.py "$CSV" "$SPP"
