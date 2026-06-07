#!/usr/bin/env bash
# ============================================================================
# P1 (path-control robustness sweeps) + P2 (coverage gaps) — UNATTENDED runner.
#
# P1: prove RR_DEPTH / MAX_BOUNCES / MIN_THROUGHPUT change only variance, not the
#     converged MEAN (invariance = unbiasedness). Pure CUDA, no Mitsuba.
# P2: more cloud cameras under meadow+scattering + a low-sigma interior check,
#     CUDA-MIS vs Mitsuba-analog (systematic <= ~1e-4).
#
# Safe to re-run: idempotent (skips finished EXRs). On ANY exit it restores
# device/core/constants.cuh to the committed baseline and rebuilds, so the tree
# is never left on a sweep value.
#
# Usage:  setsid nohup bash tools/refs/p1_p2_headless.sh </dev/null \
#             >renders/p1p2_run.log 2>&1 &
# ============================================================================
set -uo pipefail
cd /home/kacper/thesis

BIN=./build/bin/Release/test_runner
PY=tools/refs/.venv/bin/python
MITS="bash tools/refs/with_jorge_mitsuba.sh tools/refs/.venv-volprim/bin/python"
RESULTS=renders/p1p2_results.txt
mkdir -p renders/p1 renders/p2
: > "$RESULTS"

ts() { date '+%H:%M:%S'; }
say() { echo "[$(ts)] $*"; }

# ---- restore baseline constants + rebuild on ANY exit (success, fail, kill) ----
cleanup() {
  say "[cleanup] restoring constants.cuh to committed baseline + rebuild"
  git checkout -- device/core/constants.cuh 2>/dev/null
  cmake --build build >/dev/null 2>&1
  say "[cleanup] done"
}
trap cleanup EXIT

build() { cmake --build build >/dev/null 2>&1 || { say "BUILD FAILED"; exit 2; }; }

# Reset to baseline, then patch exactly one knob, then rebuild. Logs the effective value.
set_knob() { # $1=RR|MB|MT  $2=value-literal
  git checkout -- device/core/constants.cuh
  case "$1" in
    RR) sed -i "s/^constexpr size_t RR_DEPTH = 5;/constexpr size_t RR_DEPTH = $2;/" device/core/constants.cuh ;;
    MB) sed -i "s/^    128;  \/\/ Mitsuba production: 64-128/    $2;  \/\/ swept/"   device/core/constants.cuh ;;
    MT) sed -i "s/^constexpr float MIN_THROUGHPUT = 1e-4f;/constexpr float MIN_THROUGHPUT = $2;/" device/core/constants.cuh ;;
  esac
  say "   knob $1=$2 :: $(grep -nE 'RR_DEPTH = |^    [0-9]+;  // |MIN_THROUGHPUT = ' device/core/constants.cuh | tr '\n' ' ')"
  build
}

# single-Gaussian render (albedo 0.99 -> deep multiple scattering; exercises all 3 knobs)
render_sg() { # $1=tag $2=spp $3=seed
  local out=renders/p1/sg_$1_seed$3.exr
  [ -s "$out" ] && { say "   skip $out"; return; }
  SG_ALBEDO=0.99 $BIN --scene single_gaussian_validation --sigma-multiplier 4 \
    --spp "$2" --seed "$3" >/dev/null 2>&1
  cp test_results/single_gaussian_validation/0000.exr "$out"
  say "   wrote $out"
}

# cloud cam0 render (albedo 0.9, constant env) for the depth-sensitive knobs
render_cloud_p1() { # $1=tag $2=spp $3=seed
  local out=renders/p1/cloud_$1_seed$3.exr
  [ -s "$out" ] && { say "   skip $out"; return; }
  SG_CAM=0 SG_ALBEDO=0.9 $BIN --scene cloud_asset_scattering --sigma-multiplier 7.5 \
    --spp "$2" --seed "$3" >/dev/null 2>&1
  cp test_results/cloud_asset_scattering/0000.exr "$out"
  say "   wrote $out"
}

# =====================================================================
# P1 — robustness sweeps
# =====================================================================
say "================= P1 START ================="
git checkout -- device/core/constants.cuh; build   # guarantee baseline build

say "P1 baseline renders (RR=5, MB=128, MT=1e-4)"
for s in 0 1 2 3; do render_sg base 4096 "$s"; done
for s in 0 1 2;   do render_cloud_p1 base 256 "$s"; done

say "P1 RR_DEPTH sweep {off=9999, 1, 10}"
for v in 9999 1 10; do
  set_knob RR "$v"
  for s in 0 1 2 3; do render_sg "RR${v}" 4096 "$s"; done
  render_cloud_p1 "RR${v}" 256 0
done

say "P1 MAX_BOUNCES sweep {32, 64, 256}"
for v in 32 64 256; do
  set_knob MB "$v"
  for s in 0 1 2 3; do render_sg "MB${v}" 4096 "$s"; done
  render_cloud_p1 "MB${v}" 256 0
done

say "P1 MIN_THROUGHPUT sweep {1e-3, 1e-5, 0} (single-G only — scene-independent cull)"
for pair in "1e-3f:MT1em3" "1e-5f:MT1em5" "0.0f:MT0"; do
  val=${pair%%:*}; tag=${pair##*:}
  set_knob MT "$val"
  for s in 0 1 2 3; do render_sg "$tag" 4096 "$s"; done
done

git checkout -- device/core/constants.cuh; build   # back to baseline for P2

say "P1 analysis -> $RESULTS"
{
  echo "######## P1 INVARIANCE (config vs baseline; |global|/SEM < ~3 => mean invariant) ########"
  echo "## single-Gaussian (albedo 0.99, 4 seeds x 4096 spp):"
  for tag in RR9999 RR1 RR10 MB32 MB64 MB256 MT1em3 MT1em5 MT0; do
    printf "  %-8s " "$tag"
    $PY tools/refs/sg_systematic.py "renders/p1/sg_${tag}_seed*.exr" "renders/p1/sg_base_seed*.exr" \
      2>/dev/null | grep -E "global:" || echo "(compare failed)"
  done
  echo "## cloud cam0 (albedo 0.9, 256 spp; cfg seed0 vs baseline seeds0-2):"
  for tag in RR9999 RR1 RR10 MB32 MB64 MB256; do
    printf "  %-8s " "$tag"
    $PY tools/refs/sg_systematic.py "renders/p1/cloud_${tag}_seed0.exr" "renders/p1/cloud_base_seed*.exr" \
      2>/dev/null | grep -E "global:" || echo "(compare failed)"
  done
} >> "$RESULTS"
say "================= P1 DONE ================="

# =====================================================================
# P2 — coverage gaps (CUDA-MIS vs Mitsuba-analog)
# =====================================================================
say "================= P2 START ================="
P2_CAMS="6 12 18"
P2_SEEDS="0 1 2"
P2_SPP=512

# ---- (a) more cameras under meadow + scattering ----
cuda_cloud_p2() { # cam seed env sigma tag
  local out=renders/p2/cuda_$5_cam$1_seed$2.exr
  [ -s "$out" ] && { say "   skip $out"; return; }
  SG_CAM=$1 SG_ALBEDO=0.9 SG_ENV=$3 $BIN --scene cloud_asset_scattering \
    --sigma-multiplier "$4" --spp "$P2_SPP" --seed "$2" >/dev/null 2>&1
  cp test_results/cloud_asset_scattering/0000.exr "$out"
  say "   wrote $out"
}
mits_cloud_p2() { # cam seed env sigma tag srcdir
  local out=renders/p2/mits_$5_cam$1_seed$2.exr
  [ -s "$out" ] && { say "   skip $out"; return; }
  SG_CAM=$1 SG_SEED=$2 SG_ENV=$3 SG_SIGMA=$4 SG_ALBEDO=0.9 SG_HG_G=0.85 \
    SG_RFILTER=box SG_SPP=$P2_SPP $MITS tools/refs/render_cloud_prb_absorption.py >/dev/null 2>&1
  local src
  printf -v src '%s/%04d.exr' "$6" "$1"
  if [ -s "$src" ]; then cp "$src" "$out"; say "   wrote $out"; else say "   MITS MISSING $src"; fi
}

MEADOW_DIR=assets/models/cloud/refs_prb_scattering_meadow_hg0.85
say "P2(a) cameras {$P2_CAMS} meadow+scatter, ${P2_SPP}spp x seeds {$P2_SEEDS}"
for cam in $P2_CAMS; do
  for s in $P2_SEEDS; do
    cuda_cloud_p2 "$cam" "$s" meadow 7.5 meadow
    mits_cloud_p2 "$cam" "$s" meadow 7.5 meadow "$MEADOW_DIR"
  done
done

# ---- (b) low-sigma interior check (sigma=2, cam0, constant env) ----
CONST_DIR=assets/models/cloud/refs_prb_scattering_hg0.85
say "P2(b) low-sigma interior: cam0 sigma=2 const-env, ${P2_SPP}spp x seeds {$P2_SEEDS}"
for s in $P2_SEEDS; do
  cuda_cloud_p2 0 "$s" white_constant 2 lowsig
  mits_cloud_p2 0 "$s" white_constant 2 lowsig "$CONST_DIR"
done

say "P2 analysis -> $RESULTS"
{
  echo ""
  echo "######## P2 SYSTEMATIC (CUDA-MIS vs Mitsuba-analog; |global| <= ~1e-4 target) ########"
  for cam in $P2_CAMS; do
    printf "  meadow cam%-3s " "$cam"
    $PY tools/refs/sg_systematic.py "renders/p2/cuda_meadow_cam${cam}_seed*.exr" \
        "renders/p2/mits_meadow_cam${cam}_seed*.exr" 2>/dev/null \
        | grep -E "global:|median" | tr '\n' ' '; echo
  done
  printf "  lowsig cam0  "
  $PY tools/refs/sg_systematic.py "renders/p2/cuda_lowsig_cam0_seed*.exr" \
      "renders/p2/mits_lowsig_cam0_seed*.exr" 2>/dev/null \
      | grep -E "global:|median" | tr '\n' ' '; echo
} >> "$RESULTS"
say "================= P2 DONE ================="

say "ALL DONE. Results in $RESULTS"
cat "$RESULTS"
