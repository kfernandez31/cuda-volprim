#!/usr/bin/env bash
# Source Jorge's custom Mitsuba build (has BackfaceCulling needed by volprim),
# then exec the rest of the command line. The venv's Python is still used
# (so numpy/scipy/etc. are available); only Mitsuba + drjit are swapped.

source ~/jorge/mitsuba3/build/setpath.sh
export LD_LIBRARY_PATH="$HOME/jorge/mitsuba3/build:$HOME/jorge/mitsuba3/build/python/mitsuba:${LD_LIBRARY_PATH:-}"
export PYTHONPATH="$HOME/jorge/volumetric_primitives:$PYTHONPATH"

exec "$@"
