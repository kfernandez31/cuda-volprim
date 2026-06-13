#!/usr/bin/env bash
# G5b — peak VRAM per asset (the memory gap). For each asset, render with (a) its CALIBRATED binary
# and (b) the SAFE-512 binary, polling per-process VRAM (nvidia-smi --query-compute-apps=used_memory)
# to capture the peak; the delta is the VRAM the per-asset cap calibration saves. Also Mitsuba (cloud)
# for an ours-vs-Mitsuba point. Cap-immune (VRAM reservation is power-independent) → 150 W fine.
# Launch: setsid nohup bash scripts/campaign/run_g5b_vram.sh >/dev/null 2>&1 </dev/null &
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
exec > >(tee results/campaign/g5b_vram.log) 2>&1
echo "=== G5b VRAM start $(date) ==="
rm -f results/campaign/.g5b.status
OUT=results/campaign/vram.csv
echo "asset,build,caps_active_hit,peak_vram_mib,res,overflow" > "$OUT"

# poll per-process VRAM for a given PID until it exits; echo the peak MiB seen
poll_peak() {
  local pid=$1 peak=0 cur
  while kill -0 "$pid" 2>/dev/null; do
    cur=$(nvidia-smi --query-compute-apps=pid,used_memory --format=csv,noheader,nounits 2>/dev/null \
            | awk -F', *' -v p="$pid" '$1==p{print $2}')
    [ -n "${cur:-}" ] && [ "$cur" -gt "$peak" ] 2>/dev/null && peak=$cur
    sleep 0.15
  done
  echo "$peak"
}

# render <asset> with the binary currently in build/, return "peak_mib overflow"
render_and_poll() {
  local asset=$1 res log peak ovf=0
  log=$(mktemp)
  case "$asset" in
    cloud)
      res="900x600"
      SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering \
        --sigma-multiplier 7.5 --spp 16 --seed 0 >"$log" 2>&1 & ;;
    *)
      res="512x512"
      SG_PLY=assets/models/$asset/${asset}_pyr0.ply SG_ENV=meadow SG_ALBEDO=0.9 SG_RES=512 SG_VIEW=diag \
        build/bin/Release/test_runner --scene asset_validation --spp 16 --seed 0 >"$log" 2>&1 & ;;
  esac
  local pid=$!
  peak=$(poll_peak "$pid")
  wait "$pid" 2>/dev/null
  grep -q "Cap overflow:" "$log" && ovf=1
  rm -f "$log"
  echo "$peak $res $ovf"
}

declare -A CAPS=( [cloud]="64/96" [tornado]="112/384" [explosion]="32/160" [bunny]="80/528" )

for asset in cloud tornado explosion bunny; do
  for build in calibrated safe512; do
    if [ "$build" = calibrated ]; then
      cp ~/winbins/exe_$asset build/bin/Release/test_runner; cp ~/winbins/ir_$asset build/device_program.optixir
      capstr="${CAPS[$asset]}"
    else
      cp ~/winbins/exe_safe512 build/bin/Release/test_runner; cp ~/winbins/ir_safe512 build/device_program.optixir
      capstr="512/512"
    fi
    read -r peak res ovf < <(render_and_poll "$asset")
    echo "$asset,$build,$capstr,$peak,$res,$ovf" | tee -a "$OUT"
  done
done

# restore stock
cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir

# --- Mitsuba (cloud) peak VRAM, same scene, for ours-vs-Mitsuba ---
echo "--- Mitsuba cloud VRAM ---"
mlog=$(mktemp)
SG_ENV=meadow SG_CAM=0 SG_ALBEDO=0.9 SG_SIGMA=7.5 SG_SPP=16 SG_SEED=0 SG_HG_G=0.85 SG_NEE=0 \
  tools/refs/with_jorge_mitsuba.sh tools/refs/.venv/bin/python tools/refs/render_cloud_prb_absorption.py >"$mlog" 2>&1 &
mpid=$!
# Mitsuba runs under a wrapper; poll the whole-GPU compute-app set (GPU otherwise idle) for the peak
mpeak=0
while kill -0 "$mpid" 2>/dev/null; do
  c=$(nvidia-smi --query-compute-apps=used_memory --format=csv,noheader,nounits 2>/dev/null | sort -n | tail -1)
  [ -n "${c:-}" ] && [ "$c" -gt "$mpeak" ] 2>/dev/null && mpeak=$c
  sleep 0.15
done
wait "$mpid" 2>/dev/null
rm -f "$mlog"
echo "cloud,mitsuba,n/a,$mpeak,900x600,0" | tee -a "$OUT"

echo "=== G5b VRAM done $(date) ==="
cat "$OUT"
echo DONE > results/campaign/.g5b.status
