#!/usr/bin/env bash
# Build the 8 binaries for the bounce-0 precompute A/B: {stock, opt} x {cloud, tornado,
# explosion, bunny}, each at the asset's CALIBRATED caps (tab:overlap). CPU-only.
# Must be run ON branch feature/bounce0-camera-set with the code changes present
# (committed or uncommitted). "Stock" arms are produced by stashing the branch's code
# changes, building, then restoring them — same tree, fix on/off is the only delta.
# Stashes to ~/winbins/exe_b0{opt,stock}_<asset> + matching ir_.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"
exec > >(tee results/campaign/build_b0opt_pairs.log) 2>&1
echo "=== b0opt pair builds start $(date) branch=$(git branch --show-current) ==="

declare -A ACT=( [cloud]=64 [tornado]=112 [explosion]=32 [bunny]=80 )
declare -A HIT=( [cloud]=96 [tornado]=384 [explosion]=160 [bunny]=528 )

git diff --quiet -- device src include && { echo "ABORT: no code changes present (wrong branch?)"; exit 1; }

set_caps() {  # $1 active  $2 hit
  sed -i "s/#define THESIS_MAX_ACTIVE_PRIMS [0-9]*/#define THESIS_MAX_ACTIVE_PRIMS $1/;
          s/#define THESIS_HIT_BUFFER_CAPACITY [0-9]*/#define THESIS_HIT_BUFFER_CAPACITY $2/" \
      device/core/constants.cuh
}

build_stash() {  # $1 tag
  cmake --build build --target test_runner -j"$(nproc)" >/dev/null
  cp build/bin/Release/test_runner ~/winbins/exe_$1
  cp build/device_program.optixir  ~/winbins/ir_$1
  echo "stashed exe_$1"
}

for a in cloud tornado explosion bunny; do
  set_caps "${ACT[$a]}" "${HIT[$a]}"
  build_stash "b0opt_$a"                              # branch code = fix ON
  git stash push -q -u -- device src include          # fix OFF (pure main code; -u takes the new kernel files)
  set_caps "${ACT[$a]}" "${HIT[$a]}"                  # re-apply caps on stock tree
  build_stash "b0stock_$a"
  git checkout -q -- device/core/constants.cuh        # drop caps edit before pop
  git stash pop -q                                    # fix back ON
done
git checkout -q -- device/core/constants.cuh
cmake --build build --target test_runner -j"$(nproc)" >/dev/null
echo "=== done $(date); working tree restored (branch code, default caps) ==="