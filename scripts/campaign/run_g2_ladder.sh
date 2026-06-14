#!/usr/bin/env bash
# G2 merge-ladder A/B: measure each optimisation's win as the equal-spp time ratio between its merge
# commit (after) and the commit before it (before). These optimisations are image-preserving (proven),
# so equal-spp time IS the win. Old commits built in worktrees; rendered against pre-reorg asset
# symlinks (assets/cloud, assets/<env>.hdr). Interleaved rounds, clocks logged, median.
# Launch: setsid nohup bash scripts/campaign/run_g2_ladder.sh >/dev/null 2>&1 </dev/null &
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
exec > >(tee results/campaign/g2_ladder.log) 2>&1
echo "=== G2 ladder start $(date) ==="
rm -f results/campaign/.g2_ladder.status
mkdir -p results/campaign

# pre-reorg asset symlinks (harmless; old binaries need them)
ln -sfn models/cloud assets/cloud
for h in meadow_2_4k white_constant ferndale_studio_01_4k; do
  [ -f assets/environment_maps/$h.hdr ] && ln -sf environment_maps/$h.hdr assets/$h.hdr
done

# pairs: name | before-commit | after-commit
PAIRS=(
  "shadow-transmittance 9dfa6de~1 9dfa6de"
  "anyhit-fusion        9dfa6de   71ced87"
  "dedup-bounce0        3b08b0c   f54deaa"
  "skip-scan            f54deaa   174777d"
)

# --- build phase: one worktree per distinct commit ---
declare -A BIN
build_commit() {  # $1 = commit-ish ; sets BIN[$1]
  local c="$1" sha wt
  sha=$(git rev-parse --short "$c")
  wt="wt-ladder-$sha"
  BIN["$c"]="$wt/build/bin/Release/test_runner"
  if [ -x "$wt/build/bin/Release/test_runner" ]; then echo "  [cached] $c ($sha)"; return 0; fi
  git worktree list | grep -q "$wt" || git worktree add "$wt" "$sha" >/dev/null 2>&1
  echo "  building $c ($sha) ..."
  cmake -S "$wt" -B "$wt/build" -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1 \
    && cmake --build "$wt/build" --target test_runner -j"$(nproc)" >/dev/null 2>&1 \
    && [ -x "$wt/build/bin/Release/test_runner" ] && echo "  built $c" || { echo "  BUILD FAIL $c"; BIN["$c"]=""; }
}
echo "--- build phase ---"
for p in "${PAIRS[@]}"; do set -- $p; build_commit "$2"; build_commit "$3"; done
# reuse the already-built skip-after if present
[ -x wt-ladder-skipscan-after/build/bin/Release/test_runner ] && BIN["174777d"]="wt-ladder-skipscan-after/build/bin/Release/test_runner"

# --- timing phase ---
SPP=64; ROUNDS=5
CLK=results/campaign/clk_g2ladder.log
nvidia-smi --query-gpu=clocks.sm --format=csv,noheader,nounits -lms 250 > "$CLK" & SENT=$!
trap 'kill $SENT 2>/dev/null' EXIT
now(){ date +%s.%N; }
render(){ # $1=binary -> echoes seconds (or NA)
  local o; o=$(SG_ENV=meadow SG_CAM=0 "$1" --scene cloud_asset_scattering --sigma-multiplier 7.5 --spp $SPP --seed "$2" 2>&1)
  grep -oE "Total time: [0-9.]+s" <<<"$o" | grep -oE "[0-9.]+" || echo NA
}
echo "pair,before_med_s,after_med_s,ratio,before_mean,after_mean,n" > results/campaign/g2_ladder.csv
echo "--- timing phase (spp=$SPP, $ROUNDS interleaved rounds) ---"
for p in "${PAIRS[@]}"; do
  set -- $p; name="$1"; bb="${BIN[$2]:-}"; ba="${BIN[$3]:-}"
  if [ -z "$bb" ] || [ -z "$ba" ] || [ ! -x "$bb" ] || [ ! -x "$ba" ]; then
    echo "$name: SKIP (build missing)"; echo "$name,NA,NA,NA,NA,NA,0" >> results/campaign/g2_ladder.csv; continue
  fi
  bt=(); at=()
  for r in $(seq 1 $ROUNDS); do
    bt+=("$(render "$bb" $r)"); at+=("$(render "$ba" $r)")
  done
  # image-equivalence spot check (means of last render's 0000.exr can't be read here; use time medians)
  med(){ printf '%s\n' "$@" | grep -vx NA | sort -n | awk '{a[NR]=$1} END{print (NR? a[int((NR+1)/2)] : "NA")}'; }
  bm=$(med "${bt[@]}"); am=$(med "${at[@]}")
  ratio=$(awk -v b="$bm" -v a="$am" 'BEGIN{ if(a>0 && b!="NA" && a!="NA") printf "%.3f", b/a; else print "NA"}')
  echo "$name: before ${bm}s  after ${am}s  ratio ${ratio}x  (before=[${bt[*]}] after=[${at[*]}])"
  echo "$name,$bm,$am,$ratio,,,$ROUNDS" >> results/campaign/g2_ladder.csv
done
echo "clk: $(sort -n "$CLK" | awk 'NR==1{m=$1}{a[NR]=$1}END{print "min="m" p50="a[int(NR/2)]" max="a[NR]}')"
echo "=== G2 ladder done $(date) ==="
cat results/campaign/g2_ladder.csv
echo DONE > results/campaign/.g2_ladder.status
