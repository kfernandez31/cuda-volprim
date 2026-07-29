#!/usr/bin/env bash
# Phase-0 GATE (advisor meeting 2026-06-25): furnace bias-vs-spp.
# Furnace = albedo 1, white-constant env (=1), single Gaussian -> mean MUST be 1.0 at ANY spp
# (energy conservation, parameter-free analytic GT). An unbiased estimator's mean is 1.0 regardless
# of spp; only variance shrinks. So: NEE centre over-count FLAT across spp (CI excludes 0) => BIAS;
# decays toward 1.0 => convergence (advisors right). Controls (ours-MIS, Mitsuba-analog) must stay 1.0.
# max_depth=256 on every arm so albedo=1 (no absorption termination) paths don't truncate.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
OUT=results/campaign/furnace_spp
mkdir -p "$OUT/ours_exr"
grep -qxF "results/campaign/furnace_spp/" .gitignore || echo "results/campaign/furnace_spp/" >> .gitignore
CSV="$OUT/furnace_bias_vs_spp.csv"
LOG="$OUT/sweep_$(date +%H%M).log"; exec > >(tee "$LOG") 2>&1
echo "=== furnace spp sweep start $(date) ==="

SPPS="64 256 1024 4096"
SEEDS="0 1 2 3 4 5 6 7"
SIGMAS="6 12"
DEPTH=256
rm -f "$CSV"   # fresh run (cheap enough); re-run if interrupted

# ---- OURS-MIS control (native binary, low per-call overhead) ----
echo "--- ours-MIS furnace (control, must be ~1.0) ---"
for sigma in $SIGMAS; do
  for spp in $SPPS; do
    for seed in $SEEDS; do
      SG_ALBEDO=1.0 SG_ENV=white_constant build/bin/Release/test_runner \
        --scene single_gaussian_validation --sigma-multiplier "$sigma" --spp "$spp" \
        --seed "$seed" --max-depth "$DEPTH" >/dev/null 2>&1
      cp test_results/single_gaussian_validation/0000.exr \
         "$OUT/ours_exr/ours_s${sigma}_spp${spp}_seed${seed}.exr"
    done
  done
done

# ours EXRs -> CSV (mean + central-1/4-box centre; header created here)
experiments/mitsuba-reference/.venv/bin/python - "$CSV" "$OUT/ours_exr" <<'PY'
import OpenEXR, Imath, numpy as np, glob, os, sys, csv, re
csv_path, d = sys.argv[1], sys.argv[2]
def load(p):
    f=OpenEXR.InputFile(p); dw=f.header()['dataWindow']
    w=dw.max.x-dw.min.x+1; h=dw.max.y-dw.min.y+1; pt=Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c,pt),np.float32).reshape(h,w) for c in('R','G','B')],-1)
def centre(a):
    g=a.mean(-1); h,w=g.shape; return float(g[h//2-h//8:h//2+h//8, w//2-w//8:w//2+w//8].mean())
rows=[]
for p in sorted(glob.glob(os.path.join(d,'ours_*.exr'))):
    m=re.search(r'ours_s([\d.]+)_spp(\d+)_seed(\d+)', os.path.basename(p))
    sigma,spp,seed=m.group(1),int(m.group(2)),int(m.group(3))
    a=load(p); rows.append(["ours", f"{float(sigma):.3f}", spp, seed, f"{a.mean():.6f}", f"{centre(a):.6f}"])
new=not os.path.exists(csv_path)
with open(csv_path,"a",newline="") as f:
    w=csv.writer(f)
    if new: w.writerow(["arm","sigma","spp","seed","mean","centre"])
    w.writerows(rows)
print(f"ours: appended {len(rows)} rows")
PY

# ---- Mitsuba NEE + analog (one JIT-amortized process per (arm, sigma); appends to same CSV) ----
for sigma in $SIGMAS; do
  echo "--- mits-NEE furnace sigma=$sigma (UNDER TEST) ---"
  SG_ARM=mits_nee SG_NEE=1 SG_ALBEDO=1.0 SG_ENV=white_constant SG_SIGMA="$sigma" \
    SG_MAX_DEPTH="$DEPTH" SG_SHAPE=ellipsoids SG_SPPS="$SPPS" SG_SEEDS="$SEEDS" SG_CSV="$CSV" \
    experiments/mitsuba-reference/with_jorge_mitsuba.sh experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/render_single_gaussian_via_prb.py
  echo "--- mits-analog furnace sigma=$sigma (control, must be ~1.0) ---"
  SG_ARM=mits_analog SG_NEE=0 SG_ALBEDO=1.0 SG_ENV=white_constant SG_SIGMA="$sigma" \
    SG_MAX_DEPTH="$DEPTH" SG_SHAPE=ellipsoids SG_SPPS="$SPPS" SG_SEEDS="$SEEDS" SG_CSV="$CSV" \
    experiments/mitsuba-reference/with_jorge_mitsuba.sh experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/render_single_gaussian_via_prb.py
done

echo "=== furnace spp sweep done $(date) ==="
echo "CSV: $CSV ; rows: $(wc -l < "$CSV")"
