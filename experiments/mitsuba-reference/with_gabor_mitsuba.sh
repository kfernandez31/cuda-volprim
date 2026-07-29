#!/usr/bin/env bash
# Like with_jorge_mitsuba.sh but puts the LATEST *Gabor Fields* volprim on PYTHONPATH (~/jorge/GaborVolumes)
# instead of the old ~/jorge/volumetric_primitives. Sources the same ellipsoids Mitsuba build. The Gabor
# code targets a newer Mitsuba/drjit than this build; tools/refs/gabor_bootstrap.py applies the faithful
# API-translation shims (see that file's header). torch (CPU) must be installed in the venv.
source ~/jorge/mitsuba3/build/setpath.sh
export LD_LIBRARY_PATH="$HOME/jorge/mitsuba3/build:$HOME/jorge/mitsuba3/build/python/mitsuba:${LD_LIBRARY_PATH:-}"
export PYTHONPATH="$HOME/jorge/GaborVolumes:$PYTHONPATH"
exec "$@"
