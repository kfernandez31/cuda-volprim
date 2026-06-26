#!/usr/bin/env bash
# A (harden #9): furnace NEE over-count across volprim_prb SETTINGS -- rule out a config artifact.
#   shape (exact ellipsoids vs tessellated mesh), solver (bisection vs newton), hide_emitters, kernel.
#   If any setting drives the over-count to ~0, the bias was a config choice; if all stay ~+9.7%, it's
#   intrinsic. analog control must stay 0. sigma 6, 256 spp, 4 seeds, max_depth 256, albedo 1.
# B5 (fig 5.2): render the single-Gaussian absorption reference in Mitsuba at matched 3-sigma extent.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
OUT=results/campaign/furnace_spp; mkdir -p "$OUT"
CSV="$OUT/furnace_settings_ab.csv"; rm -f "$CSV"
LOG="$OUT/settings_ab_$(date +%H%M).log"; exec > >(tee "$LOG") 2>&1
echo "=== A: furnace settings A/B start $(date) ==="
COMMON=(SG_ALBEDO=1.0 SG_ENV=white_constant SG_SIGMA=6 SG_MAX_DEPTH=256 SG_SPPS=256 SG_SEEDS="0 1 2 3" SG_CSV="$CSV")
runv() { local arm="$1"; shift
  env "${COMMON[@]}" SG_ARM="$arm" "$@" \
    tools/refs/with_jorge_mitsuba.sh tools/refs/.venv/bin/python tools/refs/render_single_gaussian_via_prb.py \
    2>&1 | grep -iE "appended|RESULT" | tail -2
}
runv nee_ellipsoids SG_NEE=1 SG_SHAPE=ellipsoids
runv nee_mesh       SG_NEE=1
runv nee_newton     SG_NEE=1 SG_SHAPE=ellipsoids SG_SOLVER=newton
runv nee_hideemit   SG_NEE=1 SG_SHAPE=ellipsoids SG_HIDE_EMITTERS=1
runv nee_epanech    SG_NEE=1 SG_SHAPE=ellipsoids SG_KERNEL=epanechnikov
runv analog_ellipsoids SG_NEE=0 SG_SHAPE=ellipsoids

echo "=== B5: fig 5.2 single-Gaussian reference via Mitsuba (absorption, extent=3, exact ellipsoids, M=1) ==="
SG_ALBEDO=0 SG_ENV=white_constant SG_SIGMA=1.0 SG_MAX_DEPTH=256 SG_SHAPE=ellipsoids SG_RFILTER=box \
  SG_SPP=256 SG_SEED=0 \
  tools/refs/with_jorge_mitsuba.sh tools/refs/.venv/bin/python tools/refs/render_single_gaussian_via_prb.py \
  2>&1 | grep -iE "wrote|mean" | tail -2
cp test_results/single_gauss/mitsuba_volprim_prb_M=1.000_spp256.exr results/campaign/ladder/abs_single_ref.exr \
  && echo "B5 ref -> results/campaign/ladder/abs_single_ref.exr"
echo "=== done $(date) ==="
