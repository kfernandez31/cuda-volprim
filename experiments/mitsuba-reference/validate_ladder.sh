#!/usr/bin/env bash
# Regression gate (PLAN.md). Run after ANY change that could affect correctness/energy,
# BEFORE merging. Reference-free where possible so it works for any compile-time config
# (HG_G, ENABLE_MIS, ENABLE_NEE). Exits nonzero if any rung fails.
#
# Primary gate = the FURNACE energy invariant: a conservative medium (albedo=1) in a
# constant field must render flat (L=L_env) for ANY phase/MIS setting. This is what
# caught the MIS −6.8% energy bug (FINDINGS §8.10) and Mitsuba's NEE +6.5% (§8.1).
#
# Optional rung = single-Gaussian meadow systematic vs stored Mitsuba-analog seeds, IF
# present (build config must match the stored refs' g). Skipped with a warning otherwise.
#
# Usage: bash experiments/mitsuba-reference/validate_ladder.sh            # gate the current build
#        REBUILD=1 bash experiments/mitsuba-reference/validate_ladder.sh  # cmake --build first
set -uo pipefail
cd /home/kacper/thesis
BIN=./build/bin/Release/test_runner
PY=experiments/mitsuba-reference/.venv/bin/python
SPP="${SPP:-1024}"
fails=0

if [ "${REBUILD:-0}" = "1" ]; then
  echo "── rebuilding ──"; cmake --build build >/dev/null 2>&1 || { echo "BUILD FAILED"; exit 2; }
fi

run_furnace () {  # $1 = label, $2 = sigma
  local label="$1" sigma="$2"
  SG_ALBEDO=1.0 $BIN --scene single_gaussian_validation --sigma-multiplier "$sigma" --spp "$SPP" \
    >/dev/null 2>&1
  echo "── furnace [$label] (albedo=1, white const, σ=$sigma) ──"
  if $PY experiments/mitsuba-reference/furnace_check.py test_results/single_gaussian_validation/0000.exr 1.0; then
    :; else fails=$((fails+1)); fi
}

# Energy gate at two optical depths (thin + thick — thick exercises multiple scattering,
# where energy-loss bugs like MIS compound).
run_furnace "thin"  2
run_furnace "thick" 6

# Optional: single-Gaussian meadow bias check vs stored Mitsuba-analog seeds.
# Pick the reference dir whose g matches the current build is the caller's responsibility;
# default tries the isotropic set, then HG.
for refdir in renders/sg_meadow_hg085 renders/sg_meadow; do  # prefer HG ref (default build HG_G=0.85)
  if ls "$refdir"/mits_seed*.exr >/dev/null 2>&1; then
    echo "── single-Gaussian meadow vs $refdir (2 fresh CUDA seeds, coarse bias check) ──"
    tmp=$(mktemp -d)
    for S in 0 1; do
      SG_ENV=meadow SG_ALBEDO=0.9 $BIN --scene single_gaussian_validation \
        --sigma-multiplier 4 --spp 2048 --seed "$S" >/dev/null 2>&1
      cp test_results/single_gaussian_validation/0000.exr "$tmp/cuda_seed0$S.exr"
    done
    $PY experiments/mitsuba-reference/sg_systematic.py "$tmp/cuda_seed*.exr" "$refdir/mits_seed*.exr" | \
      grep -E "global:|median" || true
    rm -rf "$tmp"
    echo "  (interpret: median|diff| should be ~1e-3; global within a few σ — fireflies inflate global)"
    break
  fi
done

echo ""
if [ "$fails" -eq 0 ]; then echo "✅ GATE PASS (energy rungs flat)"; exit 0
else echo "❌ GATE FAIL ($fails energy rung(s))"; exit 1; fi
