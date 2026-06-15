#!/usr/bin/env bash
# Lock the RTX 3090 to the campaign's reproducible timing configuration (matches all banked timings):
#   power 350 W, persistence on, SM clock 1800 MHz, memory clock 9751 MHz.
# Clocks require root -> the privileged calls use sudo (you'll be prompted).
#
# Usage:
#   bash scripts/campaign/lock_clocks.sh           # lock (default)
#   bash scripts/campaign/lock_clocks.sh lock
#   bash scripts/campaign/lock_clocks.sh unlock     # release locks, back to default power
#   bash scripts/campaign/lock_clocks.sh status     # just print current state
#
# NOTE: 1800 MHz is the campaign's locked SM target (not the 2100 max boost) — boost is not
# thermally sustainable under load, so it locks at 1800 for reproducibility (it may still pull
# down to ~1600-1780 under sustained load; that's expected and matches prior runs).
set -uo pipefail
GPU=0
SM_CLK=1800
MEM_CLK=9751
PL=350

show() {
  echo "--- GPU $GPU state ---"
  nvidia-smi -i "$GPU" --query-gpu=persistence_mode,power.limit,power.draw,clocks.sm,clocks.max.sm,clocks.mem,clocks.applications.gr,utilization.gpu,temperature.gpu \
    --format=csv,noheader
}

MODE="${1:-lock}"
case "$MODE" in
  lock)
    echo "=== Locking GPU $GPU for timing (350 W / SM ${SM_CLK} / mem ${MEM_CLK}) ==="
    sudo nvidia-smi -i "$GPU" -pm 1                    # persistence mode on
    sudo nvidia-smi -i "$GPU" -pl "$PL"               # power limit 350 W
    sudo nvidia-smi -i "$GPU" -lgc "${SM_CLK},${SM_CLK}"   # lock graphics/SM clock
    sudo nvidia-smi -i "$GPU" -lmc "${MEM_CLK},${MEM_CLK}" # lock memory clock
    echo "=== locked. verify below (clocks.applications.gr should read ${SM_CLK}) ==="
    show
    ;;
  unlock)
    echo "=== Releasing clock locks on GPU $GPU ==="
    sudo nvidia-smi -i "$GPU" -rgc                    # reset graphics clock
    sudo nvidia-smi -i "$GPU" -rmc                    # reset memory clock
    # leave power limit where it is; uncomment to drop back to a default:
    # sudo nvidia-smi -i "$GPU" -pl 150
    echo "=== unlocked. state: ==="
    show
    ;;
  status)
    show
    ;;
  *)
    echo "usage: $0 [lock|unlock|status]"; exit 1
    ;;
esac
