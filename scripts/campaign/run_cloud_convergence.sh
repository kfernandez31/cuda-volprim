#!/usr/bin/env bash
# Phase-1 (advisor meeting 2026-06-25): cloud-meadow convergence ladder for
#  - Exp2 disagreement test: do Mitsuba-NEE and Mitsuba-analog means converge to the same value
#    as spp rises? (persistent gap => one is biased; the furnace says NEE is.)
#  - Exp3 equal-variance plots: variance vs samples, and variance vs wall-time (committee ask).
# Arms: ours-MIS, Mitsuba-analog (SG_NEE=0), Mitsuba-NEE (SG_NEE=1). Showcase config: meadow env,
# sigma 7.5, albedo 0.9, HG 0.85, max_depth 128, BOX filter (matches ours). cam 0. 8 seeds.
# 64-spp point is reused from results/campaign/g1_seeds (16 seeds) downstream; here we render 256+1024.
# Idempotent: skips existing EXRs. Times: ours 'Total time'; Mitsuba steady-state RENDER_TIME_S.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
OUT=results/campaign/cloud_conv; mkdir -p "$OUT"
grep -qxF "results/campaign/cloud_conv/" .gitignore || echo "results/campaign/cloud_conv/" >> .gitignore
TIMES="$OUT/times.csv"; [ -f "$TIMES" ] || echo "arm,spp,seed,time_s" > "$TIMES"
LOG="$OUT/conv_$(date +%H%M).log"; exec > >(tee "$LOG") 2>&1
echo "=== cloud convergence start $(date) ==="

SPPS="${SPPS:-256 1024}"
SEEDS="${SEEDS:-1 2 3 4 5 6 7 8}"
MITS_ANALOG_DIR=assets/models/cloud/refs_prb_scattering_meadow_hg0.85
MITS_NEE_DIR=assets/models/cloud/refs_prb_scattering_meadow_hg0.85_nee

for spp in $SPPS; do
  for seed in $SEEDS; do
    # ---- ours-MIS ----
    f="$OUT/ours_spp${spp}_seed${seed}.exr"
    if [ ! -f "$f" ]; then
      o=$(SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering \
            --sigma-multiplier 7.5 --spp "$spp" --seed "$seed" 2>&1)
      grep -q "Cap overflow:" <<<"$o" && echo "!!! OVERFLOW ours spp$spp seed$seed"
      t=$(grep -oE "Total time: [0-9.]+s" <<<"$o" | grep -oE "[0-9.]+")
      cp test_results/cloud_asset_scattering/0000.exr "$f"
      echo "ours,$spp,$seed,${t:-NA}" | tee -a "$TIMES"
    fi
    # ---- Mitsuba-analog ----
    f="$OUT/mits_analog_spp${spp}_seed${seed}.exr"
    if [ ! -f "$f" ]; then
      m=$(SG_ENV=meadow SG_CAM=0 SG_ALBEDO=0.9 SG_SIGMA=7.5 SG_SPP="$spp" SG_SEED="$seed" \
            SG_HG_G=0.85 SG_NEE=0 SG_MAX_DEPTH=128 SG_RFILTER=box \
            experiments/mitsuba-reference/with_jorge_mitsuba.sh experiments/mitsuba-reference/.venv/bin/python \
            experiments/mitsuba-reference/render_cloud_prb_absorption.py 2>&1)
      t=$(grep -oE "RENDER_TIME_S [0-9.]+" <<<"$m" | grep -oE "[0-9.]+" | head -1)
      cp "$MITS_ANALOG_DIR/0000.exr" "$f"
      echo "mits_analog,$spp,$seed,${t:-NA}" | tee -a "$TIMES"
    fi
    # ---- Mitsuba-NEE ----
    f="$OUT/mits_nee_spp${spp}_seed${seed}.exr"
    if [ ! -f "$f" ]; then
      m=$(SG_ENV=meadow SG_CAM=0 SG_ALBEDO=0.9 SG_SIGMA=7.5 SG_SPP="$spp" SG_SEED="$seed" \
            SG_HG_G=0.85 SG_NEE=1 SG_MAX_DEPTH=128 SG_RFILTER=box \
            experiments/mitsuba-reference/with_jorge_mitsuba.sh experiments/mitsuba-reference/.venv/bin/python \
            experiments/mitsuba-reference/render_cloud_prb_absorption.py 2>&1)
      t=$(grep -oE "RENDER_TIME_S [0-9.]+" <<<"$m" | grep -oE "[0-9.]+" | head -1)
      cp "$MITS_NEE_DIR/0000.exr" "$f"
      echo "mits_nee,$spp,$seed,${t:-NA}" | tee -a "$TIMES"
    fi
  done
done
echo "=== cloud convergence done $(date) ==="
