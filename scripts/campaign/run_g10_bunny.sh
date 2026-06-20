#!/usr/bin/env bash
# G10 parity gate for the BUNNY (cap-immune energy-ratio of converged means): ours vs Mitsuba-analog.
# Mirrors run_g10_parity.sh (tornado/explosion). Uniform albedo 0.9 scattering, white-constant env,
# matched camera (diag, dist 3.5, fov 40), 512^2, 256 spp. OURS = our PLY + asset_validation (exe_bunny);
# MITSUBA = native Gaussian-fit PLY (opacities_0->sigma_t_0 renamed) + volprim_prb analog (SG_NEE=0).
# Mean is flip-invariant + phase/cap-robust, so the ratio is the energy-parity check.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
TS=$(date +%H%M)
exec > >(tee results/campaign/g10_bunny_${TS}.log) 2>&1
echo "=== bunny parity start $(date) ==="
SIGMA=10; RES=512; SPP=256; ALB=0.9
OUR_PLY=assets/models/bunny/bunny_pyr0.ply
NAT_SRC=assets/models/bunny/bunny_gauss_1024x24k/optimized_asset_pyr0/data/root.primitives_pyr0.ply
NAT_PLY=assets/models/bunny/bunny_gauss_1024x24k/optimized_asset_pyr0/data/root.primitives_pyr0_sigmat.ply

if [ ! -f "$NAT_PLY" ]; then
  python3 -c "
src=open('$NAT_SRC','rb').read()
out=src.replace(b'property float opacities_0', b'property float sigma_t_0')
assert out!=src and len(out)==len(src)-2, 'header rename failed'
open('$NAT_PLY','wb').write(out); print('generated _sigmat PLY (opacities_0 -> sigma_t_0)')"
fi

emean(){ tools/refs/.venv/bin/python -c "import OpenEXR,Imath,numpy as np,sys
f=OpenEXR.InputFile(sys.argv[1])
pt=Imath.PixelType(Imath.PixelType.FLOAT)
a=np.stack([np.frombuffer(f.channel(c,pt),np.float32) for c in ('R','G','B')])
print(f'{a.mean():.6f}')" "$1"; }

echo "--- OURS (exe_bunny, asset_validation, $SPP spp) ---"
cp ~/winbins/exe_bunny build/bin/Release/test_runner; cp ~/winbins/ir_bunny build/device_program.optixir
oout=$(SG_PLY=$OUR_PLY SG_ENV=white_constant SG_ALBEDO=$ALB SG_RES=$RES SG_VIEW=diag \
  build/bin/Release/test_runner --scene asset_validation --spp $SPP --sigma-multiplier $SIGMA --seed 1 2>&1)
grep -q "Cap overflow:" <<<"$oout" && echo "!!! CAP OVERFLOW ours bunny: $(grep 'Cap overflow:' <<<"$oout"|head -1)"
cp test_results/asset_validation/0000.exr results/campaign/g10_bunny_ours.exr
om=$(emean results/campaign/g10_bunny_ours.exr); echo "ours mean=$om"

echo "--- MITSUBA analog (volprim_prb, NEE off) ---"
SG_PLY=$NAT_PLY SG_ALBEDO=$ALB SG_ENV=white_constant SG_RES=$RES SG_VIEW=diag SG_SIGMA=$SIGMA SG_SPP=$SPP SG_NEE=0 \
  OUT=results/campaign/g10_bunny_mits.exr \
  tools/refs/with_jorge_mitsuba.sh tools/refs/.venv-volprim/bin/python tools/refs/render_asset_via_prb.py 2>&1 | tail -4
mm=$(emean results/campaign/g10_bunny_mits.exr); echo "mits mean=$mm"

ratio=$(python3 -c "print(f'{$om/$mm:.5f}')")
verdict=$(python3 -c "print('PASS' if abs($om/$mm-1)<0.02 else 'CHECK')")
echo "asset,our_mean,mits_mean,ratio,verdict" > results/campaign/g10_bunny_${TS}.csv
echo "bunny,$om,$mm,$ratio,$verdict" | tee -a results/campaign/g10_bunny_${TS}.csv
echo "BUNNY PARITY: ours=$om mits=$mm ratio=$ratio -> $verdict"

cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir
echo "=== bunny parity done $(date) ==="
