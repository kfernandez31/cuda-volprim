#!/usr/bin/env bash
# Pin the flat-env per-sample denominator: ours-ANALOG vs Mitsuba-analog frame TIME at the SAME
# current locked clocks. Resolves the "8.5s reused/lower-bound" caveat (Talbot/Condor/Didyk all
# circled the unpinned denominator) and reconciles the 2.85s(banked)/2.90s(thesis) flat rung (S-T3).
# Both arms: white_constant env, sigma 7.5, albedo 0.9, HG g=0.85, 64 spp, cloud native res, cam 0.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
exec > >(tee results/campaign/flat_timing_pin.log) 2>&1
echo "=== flat-timing-pin start $(date) ==="
nvidia-smi --query-gpu=clocks.sm,clocks.mem,power.limit,utilization.gpu --format=csv,noheader
CLK=results/campaign/clk_flattiming.log
nvidia-smi --query-gpu=clocks.sm --format=csv,noheader,nounits -lms 200 > "$CLK" & SENT=$!; trap 'kill $SENT 2>/dev/null' EXIT

# ---- OURS-ANALOG (exe_analog) ----
cp ~/winbins/exe_analog build/bin/Release/test_runner
cp ~/winbins/ir_analog  build/device_program.optixir
OURS_T=()
for R in 0 1 2 3 4 5; do
  o=$(SG_ENV=white_constant SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering \
        --sigma-multiplier 7.5 --spp 64 --seed "$R" 2>&1)
  grep -q "Cap overflow:" <<<"$o" && echo "!!! OVERFLOW ours rep $R"
  ot=$(grep -oE "Total time: [0-9.]+s" <<<"$o" | grep -oE "[0-9.]+")
  om=$(grep -oE "mean[= ]+[0-9.]+" <<<"$o" | head -1)
  echo "ours-analog rep $R: t=${ot}s  $om"
  [ "$R" -gt 0 ] && OURS_T+=("$ot")   # discard rep 0 (warmup)
done

# ---- MITSUBA-ANALOG (patched render_cloud_prb_absorption.py prints RENDER_TIME_S) ----
MITS_T=()
for R in 0 1 2 3 4; do
  m=$(SG_ENV=white_constant SG_CAM=0 SG_ALBEDO=0.9 SG_SIGMA=7.5 SG_SPP=64 SG_SEED="$R" SG_HG_G=0.85 SG_NEE=0 \
        experiments/mitsuba-reference/with_jorge_mitsuba.sh experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/render_cloud_prb_absorption.py 2>&1)
  mr=$(grep -oE "RENDER_TIME_S [0-9.]+" <<<"$m" | grep -oE "[0-9.]+" | head -1)
  echo "mits-analog rep $R: render=${mr}s"
  [ -n "${mr:-}" ] && MITS_T+=("$mr")
done

cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir
echo "clock: $(sort -n "$CLK"|awk 'NR==1{m=$1}{a[NR]=$1}END{print "min="m" p50="a[int(NR/2)]" max="a[NR]}')"

# ---- medians + ratio ----
python3 - "${OURS_T[*]}" "${MITS_T[*]}" <<'PY'
import sys, statistics as st
ours=[float(x) for x in sys.argv[1].split()]
mits=[float(x) for x in sys.argv[2].split()]
om=st.median(ours); mm=st.median(mits)
print(f"\nOURS-ANALOG flat: reps={ours} median={om:.3f}s")
print(f"MITS-ANALOG flat: reps={mits} median={mm:.3f}s")
print(f"per-sample speedup (mits/ours) = {mm/om:.2f}x")
PY
echo "=== flat-timing-pin done $(date) ==="
