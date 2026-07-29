#!/usr/bin/env bash
# Supervisor furnace — pre-registered protocol (docs/superpowers/specs/2026-07-06-supervisor-furnace-design.md).
# Phases: 0 provenance echo | 1 medium calibration (albedo=0) | 2 mains (sigma0.1-equiv, 16384spp x 8 seeds,
# 5 arms) | 3 probes (sigma=6, gabor NEE: solver iters/type, backend, film accumulation, emitter path).
# All correctness, no timing -> any power cap OK. Stats/plots: scripts/plots/supervisor_furnace.py.
set -uo pipefail; cd "$(git rev-parse --show-toplevel)"
OUT=results/campaign/furnace_supervisor; EXR="$OUT/exr"; mkdir -p "$OUT" "$EXR"
grep -qxF "results/campaign/furnace_supervisor/" .gitignore || echo "results/campaign/furnace_supervisor/" >> .gitignore
CSV="$OUT/furnace_supervisor.csv"
LOG="$OUT/run_$(date +%m%d_%H%M).log"; exec > >(tee -a "$LOG") 2>&1
PY=experiments/mitsuba-reference/.venv/bin/python
GABOR() { experiments/mitsuba-reference/with_pip_gabor.sh "$@"; }      # official pip mitsuba 3.8.0 (canonical target)
OLD()   { experiments/mitsuba-reference/with_jorge_mitsuba.sh "$@"; }  # custom build + old volprim on PYTHONPATH
export VOLPRIM_DIR="$HOME/jorge/GaborVolumes"
TAU_SIGMA=0.1          # supervisor's medium: sigma_t 0.1 in the ours/old convention
PROBE_SIGMA=6          # probes run where the known -0.63% residual lives
SEEDS8="0 1 2 3 4 5 6 7"

centre_of_exr() {  # $1 = exr -> prints "mean centre corner"
  "$PY" - "$1" <<'PYEOF'
import sys, numpy as np, OpenEXR, Imath
f=OpenEXR.InputFile(sys.argv[1]); dw=f.header()["dataWindow"]
w=dw.max.x-dw.min.x+1; h=dw.max.y-dw.min.y+1
pt=Imath.PixelType(Imath.PixelType.FLOAT)
a=np.stack([np.frombuffer(f.channel(c,pt),np.float32).reshape(h,w) for c in ("R","G","B")],-1).astype(np.float64)
g=a.mean(-1)
c=g[h//2-h//8:h//2+h//8, w//2-w//8:w//2+w//8].mean()
k=16; corner=(g[:k,:k].mean()+g[:k,-k:].mean()+g[-k:,:k].mean()+g[-k:,-k:].mean())/4
print(f"{g.mean():.8f} {c:.8f} {corner:.8f}")
PYEOF
}

phase0() {
  echo "##### PHASE 0: provenance $(date) #####"
  echo "thesis repo: $(git rev-parse --short HEAD) (dirty files: $(git status --short | wc -l))"
  echo "old volprim: $(git -C ~/jorge/volumetric_primitives rev-parse --short HEAD 2>/dev/null)"
  echo "gabor volprim: $(git -C ~/jorge/GaborVolumes rev-parse --short HEAD 2>/dev/null)"
  "$PY" -c "import mitsuba,drjit;print('pip mitsuba',mitsuba.__version__,'drjit',drjit.__version__)"
  nvidia-smi --query-gpu=name,power.limit --format=csv,noheader
  echo "ours: fast-erf OFF (default), fast-math ON (unconditional device build, documented control-arm caveat)"
}

phase1() {
  echo "##### PHASE 1: medium calibration (albedo=0, 4096 spp) $(date) #####"
  # ours at sigma 0.1
  SG_ENV=white_constant SG_ALBEDO=0 build/bin/Release/test_runner --scene single_gaussian_validation \
    --sigma-multiplier $TAU_SIGMA --phase-g 0 --spp 4096 --seed 0 >/dev/null 2>&1
  read m c k < <(centre_of_exr test_results/single_gaussian_validation/0000.exr)
  echo "CAL ours sigma=$TAU_SIGMA centre=$c corner(env readback)=$k"
  # old volprim at sigma_t 0.1 (THE ANCHOR)
  ANCHOR=$(SG_ARM=cal_old SG_NEE=0 SG_ALBEDO=0 SG_SIGMA=$TAU_SIGMA SG_MAX_DEPTH=-1 SG_RFILTER=box \
    SG_SPP=4096 SG_SEED=0 OLD "$PY" experiments/mitsuba-reference/render_single_gaussian_via_prb.py 2>/dev/null \
    | grep -oE "centre=[0-9.]+" | grep -oE "[0-9.]+")
  echo "CAL old-volprim sigma=$TAU_SIGMA centre=$ANCHOR   <- anchor"
  # gabor: two-point linear fit in tau=-ln(centre) vs opacity, then verify
  local t1 t2 c1 c2
  c1=$(SG_NEE=0 SG_ALBEDO=0 SG_SIGMA=50 SG_MAX_DEPTH=-1 SG_SPP=4096 SG_SEED=0 \
        GABOR "$PY" experiments/mitsuba-reference/gabor_furnace.py 2>/dev/null | grep -oE "centre=[0-9.]+" | grep -oE "[0-9.]+")
  c2=$(SG_NEE=0 SG_ALBEDO=0 SG_SIGMA=100 SG_MAX_DEPTH=-1 SG_SPP=4096 SG_SEED=0 \
        GABOR "$PY" experiments/mitsuba-reference/gabor_furnace.py 2>/dev/null | grep -oE "centre=[0-9.]+" | grep -oE "[0-9.]+")
  GOP=$("$PY" -c "import math;t1=-math.log($c1);t2=-math.log($c2);t=-math.log($ANCHOR);print(f'{50+(100-50)*(t-t1)/(t2-t1):.3f}')")
  CVER=$(SG_NEE=0 SG_ALBEDO=0 SG_SIGMA=$GOP SG_MAX_DEPTH=-1 SG_SPP=4096 SG_SEED=0 \
        GABOR "$PY" experiments/mitsuba-reference/gabor_furnace.py 2>/dev/null | grep -oE "centre=[0-9.]+" | grep -oE "[0-9.]+")
  echo "CAL gabor: op50->centre=$c1 op100->centre=$c2 => calibrated opacity=$GOP verify centre=$CVER (target $ANCHOR)"
  echo "$GOP" > "$OUT/gabor_calibrated_opacity.txt"
  "$PY" -c "import math; d=abs(math.log($CVER)-math.log($ANCHOR))/abs(math.log($ANCHOR)); print(f'CAL relative-tau error: {100*d:.2f}% (must be <2%)'); exit(0 if d<0.02 else 1)" \
    || { echo "CALIBRATION FAILED"; exit 1; }
}

phase2() {
  echo "##### PHASE 2: mains (16384 spp x 8 seeds, 5 arms) $(date) #####"
  local GOP; GOP=$(cat "$OUT/gabor_calibrated_opacity.txt")
  # gabor NEE + analog (calibrated opacity, unlimited depth)
  for nee in 1 0; do arm=$([ $nee = 1 ] && echo gabor_nee || echo gabor_analog)
    SG_ARM=$arm SG_NEE=$nee SG_ALBEDO=1.0 SG_SIGMA="$GOP" SG_MAX_DEPTH=-1 \
      SG_SPPS=16384 SG_SEEDS="$SEEDS8" SG_CSV="$CSV" SG_OUTDIR="$EXR" \
      GABOR "$PY" experiments/mitsuba-reference/gabor_furnace.py 2>&1 | grep -E "^\[gabor_furnace|RESULT|appended"
  done
  # old volprim NEE + analog (sigma_t 0.1 direct)
  for nee in 1 0; do arm=$([ $nee = 1 ] && echo old_nee || echo old_analog)
    SG_ARM=$arm SG_NEE=$nee SG_ALBEDO=1.0 SG_SIGMA=$TAU_SIGMA SG_MAX_DEPTH=-1 SG_RFILTER=box \
      SG_SPPS=16384 SG_SEEDS="$SEEDS8" SG_CSV="$CSV" \
      OLD "$PY" experiments/mitsuba-reference/render_single_gaussian_via_prb.py 2>&1 | grep -E "RESULT|appended"
    for s in $SEEDS8; do st=$([ "$s" = 0 ] && echo "" || echo "_seed$s")
      src="test_results/single_gauss/mitsuba_volprim_prb_alb1.00_M=0.100_spp16384${st}.exr"
      [ -f "$src" ] && cp "$src" "$EXR/${arm}_sigma0.1_spp16384_seed${s}.exr"
    done
  done
  # ours (RR explicitly off, depth effectively unlimited)
  for s in $SEEDS8; do
    SG_ENV=white_constant SG_ALBEDO=1.0 build/bin/Release/test_runner --scene single_gaussian_validation \
      --sigma-multiplier $TAU_SIGMA --phase-g 0 --max-depth 100000 --rr-depth 100000 \
      --spp 16384 --seed "$s" >/dev/null 2>&1
    cp test_results/single_gaussian_validation/0000.exr "$EXR/ours_sigma0.1_spp16384_seed${s}.exr"
    read m c k < <(centre_of_exr "$EXR/ours_sigma0.1_spp16384_seed${s}.exr")
    echo "RESULT arm=ours sigma=0.100 spp=16384 seed=$s mean=$m centre=$c corner=$k"
    echo "ours,0.100,16384,$s,$m,$c" >> "$CSV"
  done
}

phase3() {
  echo "##### PHASE 3: probes at sigma=$PROBE_SIGMA (gabor NEE) $(date) #####"
  # white lat-long EXR for the emitter-path probe (generated, portable)
  "$PY" - <<'PYEOF'
import numpy as np, OpenEXR, Imath
w,h=64,32; hdr=OpenEXR.Header(w,h)
hdr['channels']={c: Imath.Channel(Imath.PixelType(Imath.PixelType.FLOAT)) for c in 'RGB'}
f=OpenEXR.OutputFile('results/campaign/furnace_supervisor/white_env.exr',hdr)
one=np.ones((h,w),np.float32).tobytes(); f.writePixels({'R':one,'G':one,'B':one}); f.close()
print("wrote white_env.exr (64x32, all 1.0)")
PYEOF
  run_probe() { # $1 arm label, rest: extra env as K=V pairs
    local arm="$1"; shift
    env "$@" SG_ARM="$arm" SG_ALBEDO=1.0 SG_SIGMA=$PROBE_SIGMA SG_MAX_DEPTH=-1 \
      SG_SPPS="${PROBE_SPP:-4096}" SG_SEEDS="${PROBE_SEEDS:-0 1 2 3}" SG_CSV="$CSV" \
      experiments/mitsuba-reference/with_pip_gabor.sh "$PY" experiments/mitsuba-reference/gabor_furnace.py 2>&1 | grep -E "RESULT|resolved"
  }
  run_probe probe_base       SG_NEE=1
  run_probe probe_iters64    SG_NEE=1 SG_SOLVER_ITERS=64
  run_probe probe_newton     SG_NEE=1 SG_SOLVER=newton
  run_probe probe_envmap     SG_NEE=1 SG_ENVMAP_FILE="$PWD/results/campaign/furnace_supervisor/white_env.exr"
  PROBE_SPP=1024 PROBE_SEEDS="0 1" run_probe probe_cuda1024 SG_NEE=1
  PROBE_SPP=1024 PROBE_SEEDS="0 1" run_probe probe_llvm1024 SG_NEE=1 SG_VARIANT=llvm_ad_rgb
  # film-accumulation A/B: one 16384-spp film vs 16x1024-spp averaged externally in fp64
  run_probe probe_film_whole SG_NEE=1 SG_SPPS=16384 SG_SEEDS="0 1"
  SG_ARM=probe_film_parts SG_NEE=1 SG_ALBEDO=1.0 SG_SIGMA=$PROBE_SIGMA SG_MAX_DEPTH=-1 \
    SG_SPPS=1024 SG_SEEDS="$(seq 200 215 | tr '\n' ' ')" SG_CSV="$CSV" \
    GABOR "$PY" experiments/mitsuba-reference/gabor_furnace.py 2>&1 | grep -E "RESULT|appended"
}

phase0; phase1; phase2; phase3
echo "##### ALL PHASES DONE $(date) — run scripts/plots/supervisor_furnace.py #####"
