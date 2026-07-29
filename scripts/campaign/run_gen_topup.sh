#!/usr/bin/env bash
# Fig 7.4 reference-arm top-up: 2 extra corrected-NEE seeds (2,3) per asset, meadow env,
# matched diag camera — brings the reference arm to 4x1024 spp (4096 effective, matching
# ours' 16x256). Idempotent; same recipe as nee_fair/jobs/job6_neefix_diag.sh.
set -uo pipefail
cd ~/thesis
say(){ echo "[$(date '+%F %T')] $*"; }
OUT=results/campaign/asset_neefix
declare -A NATIVE=( [tornado]=assets/models/unpacked/tornado/optimized_asset_pyr0/data/root.primitives_pyr0_sigmat.ply \
                    [explosion]=assets/models/unpacked/explosion/optimized_asset_pyr0/data/root.primitives_pyr0_sigmat.ply \
                    [bunny]=assets/models/unpacked/bunny_gauss_1024x24k/optimized_asset_pyr0/data/root.primitives_pyr0_sigmat.ply )
for a in tornado explosion bunny; do for s in 2 3; do
  f=$OUT/${a}_neefix_meadow_diag_spp1024_seed${s}.exr
  [ -f "$f" ] && { say "skip $f"; continue; }
  VOLPRIM_DIR=$PWD/results/campaign/nee_fix/GaborFixed SG_PLY=${NATIVE[$a]} SG_NEE=1 SG_SPP=1024 SG_SEED=$s \
    SG_VIEW=diag SG_DIST=3.5 SG_FOV=40 SG_RES=512 SG_ENV=meadow SG_OUT=$f \
    experiments/mitsuba-reference/with_pip_gabor.sh experiments/mitsuba-reference/.venv/bin/python experiments/mitsuba-reference/gabor_asset.py 2>&1 | tail -1
  say "topup meadow diag $a seed $s done"
done; done
say "GEN-TOPUP-DONE"
