#!/usr/bin/env bash
# G2 denoiser effective-RMSE: how many uniform spp does the OptiX denoiser buy? Render a 2048-spp GT,
# a 64-spp denoised frame, and a uniform spp sweep; the spp at which uniform RMSE matches the denoised
# RMSE gives the effective-sample multiplier (the tab:wins "~Nx effective" number). cloud-meadow,
# calibrated binary. Launch: setsid nohup bash scripts/campaign/run_g2_denoiser.sh >/dev/null 2>&1 </dev/null &
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
exec > >(tee results/campaign/g2_denoiser.log) 2>&1
echo "=== G2 denoiser start $(date) ==="
rm -f results/campaign/.g2_denoiser.status
OUT=results/campaign/g2_denoise; mkdir -p "$OUT"
cp ~/winbins/exe_cloud build/bin/Release/test_runner; cp ~/winbins/ir_cloud build/device_program.optixir
RD=test_results/cloud_asset_scattering

echo "--- 2048-spp GT ---"
SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering --sigma-multiplier 7.5 --spp 2048 --seed 0 2>&1 | grep -iE "total time|overflow"
cp $RD/0000.exr "$OUT/gt2048.exr"

echo "--- 64-spp denoised ---"
SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering --sigma-multiplier 7.5 --spp 64 --seed 1 --denoise 2>&1 | grep -iE "total time|overflow|denois"
cp $RD/0000_denoised.exr "$OUT/denoised64.exr"
cp $RD/0000.exr "$OUT/noisy64.exr"

echo "--- uniform spp sweep (vs GT) ---"
for S in 64 128 256 512 1024; do
  SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering --sigma-multiplier 7.5 --spp $S --seed 1 2>&1 | grep -oE "Total time: [0-9.]+s"
  cp $RD/0000.exr "$OUT/uniform_${S}.exr"
done
cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir

echo "--- RMSE analysis ---"
experiments/mitsuba-reference/.venv/bin/python - <<'PY'
import numpy as np, OpenEXR, Imath, glob, os
def load(p):
    f=OpenEXR.InputFile(p); dw=f.header()['dataWindow']
    w=dw.max.x-dw.min.x+1; h=dw.max.y-dw.min.y+1; pt=Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c,pt),np.float32).reshape(h,w) for c in('R','G','B')],-1)
D='results/campaign/g2_denoise'
gt=load(f'{D}/gt2048.exr')
rmse=lambda a:float(np.sqrt(np.mean((a-gt)**2)))
rden=rmse(load(f'{D}/denoised64.exr'))
print(f'denoised-64 RMSE vs GT = {rden:.4f}')
sweep={int(os.path.basename(p).split("_")[1].split(".")[0]):rmse(load(p)) for p in sorted(glob.glob(f'{D}/uniform_*.exr'))}
for s,r in sorted(sweep.items()): print(f'uniform-{s}: RMSE {r:.4f}')
# effective spp: uniform RMSE ~ c/sqrt(spp) -> spp_eff = 64*(rmse64/rden)^2
r64=sweep[64]
eff=64*(r64/rden)**2
print(f'noisy-64 RMSE {r64:.4f} -> denoiser effective spp ~ {eff:.0f}  (effective ~{eff/64:.0f}x)')
PY
echo "=== G2 denoiser done $(date) ==="
echo DONE > results/campaign/.g2_denoiser.status
