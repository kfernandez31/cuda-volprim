#!/usr/bin/env bash
# Mitsuba (volprim_prb analog) peak VRAM for tornado + explosion, to fill the tab:vram column.
# Matches the "ours" g5b config: meadow, 512^2, albedo 0.9, analog (SG_NEE=0); native PLY
# (opacities_0->sigma_t_0). VRAM reservation is power-independent -> cap-immune. Whole-GPU poll
# (GPU otherwise idle), as in run_g5b_vram.sh's Mitsuba arm.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
OUT=results/campaign/vram_mits_suite.csv
echo "asset,mits_peak_vram_mib" > "$OUT"
echo "=== Mitsuba VRAM suite start $(date) ==="
for a in tornado explosion; do
  NAT=assets/models/unpacked/$a/optimized_asset_pyr0/data/root.primitives_pyr0_sigmat.ply
  log=$(mktemp)
  SG_PLY="$NAT" SG_ALBEDO=0.9 SG_ENV=meadow SG_RES=512 SG_VIEW=diag SG_SIGMA=10 SG_SPP=16 SG_NEE=0 \
    OUT=results/campaign/vram_mits_${a}.exr \
    experiments/mitsuba-reference/with_jorge_mitsuba.sh experiments/mitsuba-reference/.venv-volprim/bin/python experiments/mitsuba-reference/render_asset_via_prb.py >"$log" 2>&1 &
  mpid=$!
  peak=0
  while kill -0 "$mpid" 2>/dev/null; do
    c=$(nvidia-smi --query-compute-apps=used_memory --format=csv,noheader,nounits 2>/dev/null | sort -n | tail -1)
    [ -n "${c:-}" ] && [ "$c" -gt "$peak" ] 2>/dev/null && peak=$c
    sleep 0.15
  done
  wait "$mpid" 2>/dev/null
  echo "--- $a: peak=$peak MiB ---"; tail -4 "$log"; rm -f "$log"
  echo "$a,$peak" | tee -a "$OUT"
done
echo "=== done $(date) ==="; cat "$OUT"
