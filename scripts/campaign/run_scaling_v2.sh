#!/usr/bin/env bash
# Scaling v2 (§7.6 redesign): render time vs N across three synthetic families whose
# per-ray crossing counts are pinned BY CONSTRUCTION (sheet ~O(1), cube ~N^{1/3},
# stack = N), one fixed SAFE-512 binary, absorption (deterministic), white env.
#   - 1 warm-up + 5 timed repeats per config, ALL repeats retained in the CSV.
#   - mean-T self-check per config from the written EXR (family-constant => the
#     tau normalisation held and only N varies).
#   - --measure-caps pass at each family's endpoints (mechanism check: measured
#     per-ray hit counts follow the constructed {O(1), ~k, =N}).
# Full run REQUIRES the locked operating point (>=300 W, SM pinned; §5.1 protocol).
# SMOKE=1: one config per family, 1 repeat, no power guard (functional check only).
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
TS=$(date +%H%M)
LOG=results/campaign/scaling_v2_${TS}.log
exec > >(tee "$LOG") 2>&1
echo "=== scaling_v2 start $(date) SMOKE=${SMOKE:-0} ==="

SMOKE=${SMOKE:-0}
if [ "$SMOKE" != "1" ]; then
  PW=$(nvidia-smi --query-gpu=power.limit --format=csv,noheader,nounits | cut -d. -f1)
  [ "${PW:-0}" -lt 300 ] && { echo "ABORT power ${PW}W<300 (need locked clocks; run scripts/campaign/lock_clocks.sh)"; exit 1; }
  nvidia-smi --query-gpu=clocks.sm --format=csv,noheader,nounits -lms 250 \
    > results/campaign/clk_scaling_v2_${TS}.log & SENT=$!
fi

BIN=build/bin/Release/test_runner
[ -x "$BIN" ] || { echo "ABORT: $BIN missing"; exit 1; }
# one fixed binary for the whole sweep: the safe-512 stash built from current main
[ -f ~/winbins/exe_safe512_v2 ] || { echo "ABORT: exe_safe512_v2 stash missing"; exit 1; }
cp ~/winbins/exe_safe512_v2 "$BIN"; cp ~/winbins/ir_safe512_v2 build/device_program.optixir
cleanup() {
  [ -n "${SENT:-}" ] && kill "$SENT" 2>/dev/null
  cp ~/winbins/exe_stock build/bin/Release/test_runner 2>/dev/null
  cp ~/winbins/ir_stock build/device_program.optixir 2>/dev/null
  echo "stock binary restored"
}
trap cleanup EXIT
grep -q . assets/synthetic/scaling_v2/manifest.csv || { echo "ABORT: run tools/gen_scaling_v2.py first"; exit 1; }

tt() { grep -oE "Total time: [0-9.]+s" | grep -oE "[0-9.]+" | head -1; }
meanT() { python3 - "$1" <<'PY'
import sys, numpy as np, OpenEXR, Imath
f = OpenEXR.InputFile(sys.argv[1]); dw = f.header()["dataWindow"]
sz = (dw.max.y - dw.min.y + 1, dw.max.x - dw.min.x + 1)
pt = Imath.PixelType(Imath.PixelType.FLOAT)
img = np.stack([np.frombuffer(f.channel(c, pt), np.float32).reshape(sz) for c in "RGB"], -1)
print(f"{img.mean():.5f}")
PY
}

REPEATS=5; [ "$SMOKE" = "1" ] && REPEATS=1
CSV=results/campaign/scaling_v2.csv
[ "$SMOKE" = "1" ] && CSV=results/campaign/scaling_v2_smoke.csv
# Resumable: append-only CSV; a config is written only after all its repeats finish,
# so re-running the script skips everything already measured and picks up the rest.
[ -f "$CSV" ] || echo "family,label,N,t0,t1,t2,t3,t4,t_med_s,mean_T,overflow" > "$CSV"

run_one() {  # $1 family  $2 label  $3 N
  if grep -q "^$1,$2," "$CSV"; then echo "skip $2 (already measured)"; return 0; fi
  local ply="assets/synthetic/scaling_v2/$2.ply"
  local common=(--scene asset_validation --spp 64 --sigma-multiplier 1.0 --seed 0)
  # warm-up (absorbs JIT/module load; discarded)
  SG_PLY="$ply" SG_RES=512 SG_VIEW=negz "$BIN" "${common[@]}" > /dev/null 2>&1
  local T=() ovf=0 o t
  for r in $(seq 1 "$REPEATS"); do
    o=$(SG_PLY="$ply" SG_RES=512 SG_VIEW=negz "$BIN" "${common[@]}" 2>&1)
    grep -q "Cap overflow:" <<<"$o" && ovf=1
    t=$(tt <<<"$o"); T+=("$t"); echo "  $2 repeat $r: ${t}s"
  done
  while [ "${#T[@]}" -lt 5 ]; do T+=(""); done
  local med
  med=$(printf '%s\n' "${T[@]:0:$REPEATS}" | sort -n | awk '{a[NR]=$1} END{print a[int((NR+1)/2)]}')
  local exr="test_results/asset_validation/0000.exr"
  local mt=""; [ -f "$exr" ] && mt=$(meanT "$exr")
  echo "$1,$2,$3,${T[0]},${T[1]},${T[2]},${T[3]},${T[4]},$med,$mt,$ovf" >> "$CSV"
  echo "$2 N=$3 med=${med}s meanT=$mt ovf=$ovf"
}

caps_one() {  # $1 label — mechanism check at family endpoints
  local ply="assets/synthetic/scaling_v2/$1.ply"
  echo "--- measure-caps $1 ---"
  SG_PLY="$ply" SG_RES=512 SG_VIEW=negz "$BIN" --scene asset_validation --spp 4 \
    --sigma-multiplier 1.0 --seed 0 --measure-caps 2>&1 | grep -iE "cap|hit|active" | head -8
}

if [ "$SMOKE" = "1" ]; then
  run_one sheet sheet_n8 64
  run_one cube  cube_n4 64
  run_one stack stack_N32 32
  caps_one sheet_n8; caps_one cube_n4; caps_one stack_N32
else
  while IFS=, read -r fam label k N rest; do
    [ "$fam" = "family" ] && continue
    run_one "$fam" "$label" "$N"
  done < <(cut -d, -f1,2,3,4 assets/synthetic/scaling_v2/manifest.csv)
  for l in sheet_n4 sheet_n91 cube_n2 cube_n24 stack_N8 stack_N512; do caps_one "$l"; done
fi
echo "=== scaling_v2 done $(date) ==="
