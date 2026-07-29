#!/usr/bin/env bash
# G10 parity gate (cap-immune, energy-ratio of converged means): ours vs Mitsuba-analog.
# tornado + explosion, uniform albedo 0.9 scattering, white constant env, matched camera (diag,
# dist 3.5, fov 40), 512^2, 256 spp. OURS = our PLY + asset_validation (calibrated pair);
# MITSUBA = Jorge native PLY (opacities_0->sigma_t_0 renamed) + volprim_prb analog (SG_NEE=0).
# Mean is flip-invariant, so the known camera vertical-flip does not affect the ratio.
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
TS=$(date +%H%M)
exec > >(tee results/campaign/g10_parity_${TS}.log) 2>&1
echo "=== G10 parity start $(date) ==="
rm -f results/campaign/.g10.status
SIGMA=10; RES=512; SPP=256; ALB=0.9
emean(){ experiments/mitsuba-reference/.venv/bin/python -c "import OpenEXR,Imath,numpy as np,sys
f=OpenEXR.InputFile(sys.argv[1]);dw=f.header()['dataWindow']
pt=Imath.PixelType(Imath.PixelType.FLOAT)
a=np.stack([np.frombuffer(f.channel(c,pt),np.float32) for c in ('R','G','B')])
print(f'{a.mean():.6f}')" "$1"; }
echo "asset,our_mean,mits_mean,ratio,verdict" > results/campaign/g10_parity_${TS}.csv
declare -i OVF=0
for a in tornado explosion; do
  echo "--- $a (albedo $ALB, sigma $SIGMA, $RES^2, $SPP spp) ---"
  OUR_PLY=assets/models/$a/${a}_pyr0.ply
  NAT_PLY=assets/models/unpacked/$a/optimized_asset_pyr0/data/root.primitives_pyr0_sigmat.ply
  # OURS (calibrated pair)
  cp ~/winbins/exe_$a build/bin/Release/test_runner; cp ~/winbins/ir_$a build/device_program.optixir
  oout=$(SG_PLY=$OUR_PLY SG_ENV=white_constant SG_ALBEDO=$ALB SG_RES=$RES SG_VIEW=diag \
    build/bin/Release/test_runner --scene asset_validation --spp $SPP --sigma-multiplier $SIGMA --seed 1 2>&1)
  grep -q "Cap overflow:" <<<"$oout" && { OVF+=1; echo "!!! CAP OVERFLOW ours $a: $(grep 'Cap overflow:' <<<"$oout"|head -1)"; }
  cp test_results/asset_validation/0000.exr results/campaign/g10_${a}_ours.exr
  om=$(emean results/campaign/g10_${a}_ours.exr)
  # MITSUBA analog (NEE off)
  SG_PLY=$NAT_PLY SG_ALBEDO=$ALB SG_ENV=white_constant SG_RES=$RES SG_VIEW=diag SG_SIGMA=$SIGMA SG_SPP=$SPP SG_NEE=0 \
    OUT=results/campaign/g10_${a}_mits.exr \
    experiments/mitsuba-reference/with_jorge_mitsuba.sh experiments/mitsuba-reference/.venv-volprim/bin/python experiments/mitsuba-reference/render_asset_via_prb.py >/dev/null 2>&1
  mm=$(emean results/campaign/g10_${a}_mits.exr)
  ratio=$(python3 -c "print(f'{$om/$mm:.5f}')")
  verdict=$(python3 -c "print('PASS' if abs($om/$mm-1)<0.02 else 'CHECK')")
  echo "$a: ours=$om  mits=$mm  ratio=$ratio  $verdict"
  echo "$a,$om,$mm,$ratio,$verdict" >> results/campaign/g10_parity_${TS}.csv
done
cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir
echo "OVERFLOWS: $OVF"; echo "--- results ---"; cat results/campaign/g10_parity_${TS}.csv
echo "=== G10 done $(date) ==="
echo DONE > results/campaign/.g10.status
