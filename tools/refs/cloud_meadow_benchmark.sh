#!/usr/bin/env bash
# Equal-quality speed benchmark on the REAL target scene (cloud + meadow + HG), CUDA-MIS vs
# Mitsuba-analog. Methodology = FINDINGS §8.5: equal-quality time ∝ noise_constant(k) × s/spp,
# where k = (per-seed std)² × spp (systematic-free, from the seed sets) and s/spp = wall-clock.
# Run on a CLEAN GPU (no other render contending). Assumes the seed sets in renders/cloud_meadow_hg/
# already exist (cuda_seed* = CUDA-MIS, mits_seed* = Mitsuba-analog).
set -uo pipefail
cd /home/kacper/thesis
SPP="${1:-256}"
REPS="${2:-3}"
BIN=./build/bin/Release/test_runner

echo "=== timing CUDA-MIS cloud cam0 @ ${SPP}spp × ${REPS} ==="
cuda_t=0
for r in $(seq 1 "$REPS"); do
  t=$(SG_ENV=meadow SG_CAM=0 $BIN --scene cloud_asset_scattering --sigma-multiplier 7.5 \
        --spp "$SPP" --seed $((100+r)) 2>&1 | grep -oiE "Total time: [0-9.]+s" | grep -oE "[0-9.]+")
  echo "  rep$r: ${t}s"; cuda_t=$(LC_ALL=C awk "BEGIN{print $cuda_t+$t}")
done
cuda_spp_t=$(LC_ALL=C awk "BEGIN{print $cuda_t/$REPS/$SPP}")
echo "CUDA-MIS: $(LC_ALL=C awk "BEGIN{print $cuda_t/$REPS}")s/render  -> ${cuda_spp_t}s/spp"

echo "=== timing Mitsuba-analog cloud cam0 @ ${SPP}spp × ${REPS} ==="
mits_t=0
for r in $(seq 1 "$REPS"); do
  out=$(SG_ENV=meadow SG_CAM=0 SG_ALBEDO=0.9 SG_SIGMA=7.5 SG_SPP="$SPP" SG_SEED=$((100+r)) SG_HG_G=0.85 \
        tools/refs/with_jorge_mitsuba.sh tools/refs/.venv/bin/python -c "
import time,os,sys
sys.argv=['x']
t0=time.time()
exec(open('tools/refs/render_cloud_prb_absorption.py').read())
print('WALL %.3f'%(time.time()-t0))
" 2>&1 | grep -oE "WALL [0-9.]+" | grep -oE "[0-9.]+")
  echo "  rep$r: ${out}s"; mits_t=$(LC_ALL=C awk "BEGIN{print $mits_t+$out}")
done
mits_spp_t=$(LC_ALL=C awk "BEGIN{print $mits_t/$REPS/$SPP}")
echo "Mitsuba: $(LC_ALL=C awk "BEGIN{print $mits_t/$REPS}")s/render  -> ${mits_spp_t}s/spp"

echo ""
echo "=== noise constants + equal-quality (from seed sets @ 256spp) ==="
tools/refs/.venv/bin/python - "$cuda_spp_t" "$mits_spp_t" <<'PY'
import sys, glob, numpy as np, OpenEXR, Imath
tC=float(sys.argv[1]); tM=float(sys.argv[2])
def load(p):
    f=OpenEXR.InputFile(p); dw=f.header()["dataWindow"]
    sz=(dw.max.y-dw.min.y+1,dw.max.x-dw.min.x+1)
    ch=f.channels(["R","G","B"],Imath.PixelType(Imath.PixelType.FLOAT))
    return np.stack([np.frombuffer(c,np.float32).reshape(sz) for c in ch],-1)
def k_of(glb, spp=256):
    S=[load(p) for p in sorted(glob.glob(glb))]
    std=np.mean([np.std(S[i]-S[j])/np.sqrt(2) for i in range(len(S)) for j in range(i+1,len(S))])
    return std, std*std*spp, len(S)
sC,kC,nC=k_of("renders/cloud_meadow_hg/cuda_seed*.exr")
sM,kM,nM=k_of("renders/cloud_meadow_hg/mits_seed*.exr")
print(f"CUDA-MIS : per-seed RMSE@256={sC:.4f} ({nC} seeds)  noise-const kC={kC:.4f}  t={tC:.4f}s/spp")
print(f"Mitsuba  : per-seed RMSE@256={sM:.4f} ({nM} seeds)  noise-const kM={kM:.4f}  t={tM:.4f}s/spp")
# equal-quality time to noise sigma: spp=k/sigma^2, time=spp*t  => time = (k*t)/sigma^2
# ratio Mitsuba/CUDA = (kM*tM)/(kC*tC)
eqC=kC*tC; eqM=kM*tM
print(f"\nequal-quality cost (k×t, lower=better):  CUDA-MIS={eqC:.4f}   Mitsuba={eqM:.4f}")
if eqC>0:
    r=eqM/eqC
    print(f"==> CUDA-MIS is {r:.2f}× {'FASTER' if r>1 else 'SLOWER'} than Mitsuba-analog at equal quality on cloud+meadow")
print(f"   (throughput ratio CUDA/Mitsuba = {tC/tM:.2f}× per spp; variance ratio kC/kM = {kC/kM:.3f})")
print(f"   NB heavy Mitsuba fireflies converge slower than 1/√spp, so this k×t UNDERSTATES CUDA's edge.")
PY
