#!/usr/bin/env bash
# Portable runner for the Gabor volprim furnace repro.
# Set VOLPRIM_DIR to the directory CONTAINING the `volprim/` package (your Gabor Fields checkout).
# Leave it unset if `volprim` is already importable in the active Python (e.g. `pip install -e .`).
# Uses whatever Mitsuba + drjit are importable (validated on the official mitsuba 3.8.0 + drjit 1.3.1).
#     VOLPRIM_DIR=/path/to/GaborVolumes ./with_pip_gabor.sh python gabor_furnace.py
export PYTHONPATH="${VOLPRIM_DIR:+$VOLPRIM_DIR:}$PYTHONPATH"
exec "$@"
