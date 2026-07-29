#!/usr/bin/env bash
# Fetch the CC0 Poly Haven environment maps used by the Section 6 experiments into
# assets/environment_maps/. That tree is gitignored (large binaries are not committed),
# so this makes the assets reproducible from their source. meadow_2_4k.hdr is committed
# as an exception; white_constant.hdr is generated/committed; this fetches the rest.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DST="$ROOT/assets/environment_maps"
mkdir -p "$DST"
UA="Mozilla/5.0 (thesis-asset-fetch)"

fetch() { # <polyhaven-id> [resolution]
  local id="$1" res="${2:-4k}" out="$DST/${1}_${2:-4k}.hdr"
  if [ -f "$out" ]; then echo "have   $out"; return; fi
  local url="https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/${res}/${id}_${res}.hdr"
  echo "fetch  $id ($res)"
  curl -fsSL -A "$UA" "$url" -o "$out" && echo "  ->   $out ($(du -h "$out" | cut -f1))"
}

# Mid-peak studio env for the RIS peakiness ladder (flat -> studio -> meadow). CC0.
fetch ferndale_studio_01 4k

echo "done. (meadow_2_4k.hdr is committed; white_constant.hdr is committed.)"
# Provenance note (2026-07-16): white_constant.hdr is a committed 16x16 constant-1.0 lat-long
# RGBE HDR (verified: every pixel exactly 1.0). Any constant-1 map is radiometrically equivalent;
# no generator script exists or is needed.
