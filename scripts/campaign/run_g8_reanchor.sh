#!/usr/bin/env bash
# G8 perf re-anchor: analytic (exe_cloud, 64/96) vs icosphere N=0..3 (built at 64/96),
# cloud-meadow 128 spp (matches the original 128/128 sweep), interleaved. Refreshes the Ch6
# icosphere table's absolute seconds at calibrated caps + re-confirms "icosphere faster at every N".
# Builds first (CPU, window-independent), re-checks power before the TIMED phase. Overflow-checked.
# Headless: setsid nohup bash scripts/campaign/run_g8_reanchor.sh </dev/null &
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
TS=$(date +%H%M)
exec > >(tee results/campaign/g8_reanchor_${TS}.log) 2>&1
echo "=== G8 perf re-anchor start $(date) ==="
rm -f results/campaign/.g8.status

# --- BUILD PHASE (CPU, power-independent): icosphere N=0..3 at 64/96 ---
sed -i 's/#define THESIS_MAX_ACTIVE_PRIMS [0-9]*/#define THESIS_MAX_ACTIVE_PRIMS 64/;s/#define THESIS_HIT_BUFFER_CAPACITY [0-9]*/#define THESIS_HIT_BUFFER_CAPACITY 96/' device/core/constants.cuh
echo "constants: $(grep -oE '#define THESIS_MAX_ACTIVE_PRIMS [0-9]+|#define THESIS_HIT_BUFFER_CAPACITY [0-9]+' device/core/constants.cuh | tr '\n' ' ')"
for N in 0 1 2 3; do
  echo "--- build ico N=$N (64/96) $(date +%H:%M:%S) ---"
  cmake -S . -B build-icoN$N -DCMAKE_BUILD_TYPE=Release -DTHESIS_ICOSPHERE=ON -DTHESIS_ICOSPHERE_N=$N >/dev/null 2>&1
  cmake --build build-icoN$N --target test_runner -j"$(nproc)" >/dev/null 2>&1
  [ -x build-icoN$N/bin/Release/test_runner ] || { echo "BUILD FAIL N=$N"; git checkout -- device/core/constants.cuh; echo FAIL > results/campaign/.g8.status; exit 1; }
done
git checkout -- device/core/constants.cuh
echo "all icosphere builds done $(date +%H:%M:%S)"

# analytic arm = exe_cloud (64/96, exact) installed into build/
cp ~/winbins/exe_cloud build/bin/Release/test_runner; cp ~/winbins/ir_cloud build/device_program.optixir

# --- power re-check before TIMED phase ---
PW=$(nvidia-smi --query-gpu=power.limit --format=csv,noheader,nounits | cut -d. -f1)
if [ "${PW:-0}" -lt 300 ]; then
  echo "WINDOW CLOSED during builds (${PW}W) — icosphere builds preserved (build-icoN0..3); timing deferred."
  echo BUILDS_DONE > results/campaign/.g8.status; exit 0
fi
echo "power=${PW}W — timing phase"

declare -i OVF=0
chk(){ grep -q "Cap overflow:" <<<"$1" && { OVF+=1; echo "!!! OVERFLOW $2: $(grep 'Cap overflow:' <<<"$1"|head -1)"; }; }
t_of(){ grep -oE "Total time: [0-9.]+s" <<<"$1" | grep -oE "[0-9.]+"; }
nvidia-smi --query-gpu=clocks.sm --format=csv,noheader,nounits -lms 200 > results/campaign/clk_g8_${TS}.log &
SENT=$!; trap 'kill $SENT 2>/dev/null' EXIT

declare -A BIN=( [analytic]=build/bin/Release/test_runner \
  [icoN0]=build-icoN0/bin/Release/test_runner [icoN1]=build-icoN1/bin/Release/test_runner \
  [icoN2]=build-icoN2/bin/Release/test_runner [icoN3]=build-icoN3/bin/Release/test_runner )
ARMS="analytic icoN0 icoN1 icoN2 icoN3"
echo "--- interleaved timing (cloud-meadow 128 spp, seed 1, 5 rounds) ---"
LOG=results/campaign/g8_rounds_${TS}.log; : > $LOG
for r in 1 2 3 4 5; do for a in $ARMS; do
  o=$(SG_ENV=meadow SG_CAM=0 ${BIN[$a]} --scene cloud_asset_scattering --spp 128 --seed 1 2>&1); chk "$o" "$a"
  echo "round=$r arm=$a t=$(t_of "$o")" | tee -a $LOG
done; done

experiments/mitsuba-reference/.venv/bin/python - "$LOG" <<'PY'
import sys, statistics as st, re, collections
v=collections.defaultdict(list)
for line in open(sys.argv[1]):
    m=re.search(r'arm=(\S+) t=([0-9.]+)', line)
    if m and m.group(2): v[m.group(1)].append(float(m.group(2)))
med={a:st.median(t) for a,t in v.items() if t}
base=med.get('analytic')
print(f"\n{'arm':>9}{'t_med':>9}{'rel':>8}{'analytic_pays':>14}")
for a in ['analytic','icoN0','icoN1','icoN2','icoN3']:
    if a in med:
        rel=med[a]/base
        pays='' if a=='analytic' else f"{base/med[a]:.2f}x"
        print(f"{a:>9}{med[a]:>9.3f}{rel:>8.3f}{pays:>14}")
PY

echo "clock: $(sort -n results/campaign/clk_g8_${TS}.log | awk 'NR==1{m=$1}{a[NR]=$1}END{print "min="m" p50="a[int(NR/2)]" max="a[NR]}')"
echo "OVERFLOWS: $OVF"
cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir
echo "=== G8 re-anchor done $(date) ==="
[ "$OVF" -eq 0 ] && echo DONE > results/campaign/.g8.status || echo DONE_WITH_OVERFLOWS > results/campaign/.g8.status
