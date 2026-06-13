#!/usr/bin/env bash
# G6 helper: build the wavefront path tracer ON and OFF at the deprecated-wavefront-phase1 tip (same
# commit → identical caps, so the ON/OFF ratio is valid regardless of that branch's stale cap values).
# Build-only (CPU/150 W); the render confirm runs separately. Uses a git worktree (auto-created).
# Launch headless via setsid nohup.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
ROOT=$(pwd)
exec > >(tee results/campaign/build_wavefront.log) 2>&1
echo "=== wavefront builds start $(date) ==="
rm -f results/campaign/.build_wavefront.status
WT="$ROOT/wt-wavefront"
git worktree list | grep -q "$WT" || git worktree add "$WT" deprecated-wavefront-phase1 2>&1 | tail -2
echo "worktree at $WT ($(git -C "$WT" rev-parse --short HEAD))"

# OFF = megakernel baseline, ON = wavefront. Separate build dirs (each bakes its own OPTIXIR_PATH).
cmake -S "$WT" -B "$WT/build-mega" -DCMAKE_BUILD_TYPE=Release -DTHESIS_WAVEFRONT=OFF >/dev/null 2>&1
cmake --build "$WT/build-mega" --target test_runner -j"$(nproc)" >/dev/null 2>&1 || {
  echo "FAIL: megakernel build"; echo FAIL > results/campaign/.build_wavefront.status; exit 1; }
echo "megakernel (OFF) built: $([ -x "$WT/build-mega/bin/Release/test_runner" ] && echo ok || echo MISSING)"

cmake -S "$WT" -B "$WT/build-wf" -DCMAKE_BUILD_TYPE=Release -DTHESIS_WAVEFRONT=ON >/dev/null 2>&1
cmake --build "$WT/build-wf" --target test_runner -j"$(nproc)" >/dev/null 2>&1 || {
  echo "FAIL: wavefront build"; echo FAIL > results/campaign/.build_wavefront.status; exit 1; }
echo "wavefront (ON) built: $([ -x "$WT/build-wf/bin/Release/test_runner" ] && echo ok || echo MISSING)"

echo "=== wavefront builds done $(date) ==="
echo DONE > results/campaign/.build_wavefront.status
