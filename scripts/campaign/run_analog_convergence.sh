#!/usr/bin/env bash
# Sampler-only convergence: ours-ANALOG vs Mitsuba-ANALOG on the FLAT (white-constant) env.
# Both in analog mode (no NEE/MIS on either side) so this isolates the SAMPLER, not the estimator.
# Flat env (not meadow) because meadow analog-vs-analog is firefly-metric-unstable. Fine spp ladder for
# the full variance-vs-spp / variance-vs-time trend. cloud, sigma 7.5, albedo 0.9, HG 0.85, box filter.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
OUT=results/campaign/analog_conv; mkdir -p "$OUT"
grep -qxF "results/campaign/analog_conv/" .gitignore || echo "results/campaign/analog_conv/" >> .gitignore
TIMES="$OUT/times.csv"; [ -f "$TIMES" ] || echo "arm,spp,seed,time_s" > "$TIMES"
LOG="$OUT/run_$(date +%H%M).log"; exec > >(tee "$LOG") 2>&1
echo "=== analog convergence (flat) start $(date) ==="
SPPS="${SPPS:-16 32 64 128 256 512}"; SEEDS="${SEEDS:-1 2 3 4 5 6 7 8}"
MITS_DIR=assets/models/cloud/refs_prb_scattering_hg0.85   # white_constant (no _meadow), analog (no _nee)

# ---- ours-ANALOG (exe_analog = ENABLE_NEE=false build) ----
cp ~/winbins/exe_analog build/bin/Release/test_runner; cp ~/winbins/ir_analog build/device_program.optixir
for spp in $SPPS; do for seed in $SEEDS; do
  f="$OUT/ours_analog_spp${spp}_seed${seed}.exr"
  [ -f "$f" ] && continue
  o=$(SG_ENV=white_constant SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering \
        --sigma-multiplier 7.5 --spp "$spp" --seed "$seed" 2>&1)
  t=$(grep -oE "Total time: [0-9.]+" <<<"$o" | grep -oE "[0-9.]+")
  cp test_results/cloud_asset_scattering/0000.exr "$f"
  echo "ours_analog,$spp,$seed,${t:-NA}" | tee -a "$TIMES"
done; done
cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir

# ---- Mitsuba-ANALOG (SG_NEE=0) ----
for spp in $SPPS; do for seed in $SEEDS; do
  f="$OUT/mits_analog_spp${spp}_seed${seed}.exr"
  [ -f "$f" ] && continue
  m=$(SG_ENV=white_constant SG_CAM=0 SG_ALBEDO=0.9 SG_SIGMA=7.5 SG_SPP="$spp" SG_SEED="$seed" \
        SG_HG_G=0.85 SG_NEE=0 SG_MAX_DEPTH=128 SG_RFILTER=box \
        tools/refs/with_jorge_mitsuba.sh tools/refs/.venv/bin/python tools/refs/render_cloud_prb_absorption.py 2>&1)
  t=$(grep -oE "RENDER_TIME_S [0-9.]+" <<<"$m" | grep -oE "[0-9.]+" | head -1)
  cp "$MITS_DIR/0000.exr" "$f"
  echo "mits_analog,$spp,$seed,${t:-NA}" | tee -a "$TIMES"
done; done
echo "=== analog convergence done $(date) ==="
