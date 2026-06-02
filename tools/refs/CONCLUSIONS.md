# Voxel-reference pipeline — current state and conclusions

## What we set out to do

Stop calibrating `--sigma-multiplier` against `assets/cloud/refs_pyr0/` (whose
provenance is unclear — Jorge says voxel-grid, the asset's `__init__.py`
configures a Gaussian-ellipsoid integrator). Build a tool that generates
reference renders we control, then diff DSYG output against them.

## What we built

- `tools/refs/.venv/` — isolated Python env (Mitsuba 3.8, drjit 1.3, numpy 2.4, plyfile, OpenEXR)
- `tools/refs/render_voxel_reference.py` — renders the cloud's `pyramid_level_0.npy`
  voxel grid through Mitsuba's stock `prbvolpath` + `gridvolume`, iterating all
  24 cameras from `assets/cloud/__init__.py`
- `tools/refs/compare_renders.py` — pairwise EXR diff (RMSE, MAE, silhouette IoU,
  phase-correlation shift)
- `tools/refs/requirements.txt`, `tools/refs/README.md`
- `.gitignore` entries for `tools/refs/.venv/` and `__pycache__/`
- `assets/cloud/refs_voxel_self/0000.exr` … `0023.exr` — 24 voxel-ref frames

## Bug we found and fixed mid-build

Initially passed the same `to_world` to both `cube` and `gridvolume`. Mitsuba's
`cube` spans `[-1, 1]^3` in local space, `gridvolume` spans `[0, 1]^3` — same
transform misplaces them by a 2x + offset. Fixed: IoU vs `refs_pyr0/` jumped from
0.15 to 0.60.

## Why this path stalled

The renders are *spatially wrong* — the cloud is in the wrong location or
orientation relative to where the cameras look. Visible as "obstructions" /
"messed-up perspective" rather than a clean cloud silhouette.

The likely causes are all unknown conventions:

| Question | What we'd need to know |
|---|---|
| World bbox for the voxel grid | Where was the cloud volume placed when the .npy was authored? |
| `.npy` axis order | `(250, 170, 307)` -> is that (X,Y,Z), (Z,Y,X), or something else? |
| Density-scaler interpretation | `sigmat_scale=7.5` in *what* units, relative to *what* bbox? |
| Cube vs gridvolume alignment | Should they coincide exactly, or does one extend beyond the other? |

We could guess each and check, but every "fix" risks cancelling against another
hidden mistake.

## Why we're pausing this path

Manual convention discovery is exactly the spiral the tool was meant to escape.
The cleaner unlock is Jorge's `advol` library — it's *his* voxel-grid reference
renderer, designed for this exact comparison. Its `get_volume_config(path)` and
per-asset config classes (`CloudConfig`, etc.) encapsulate all four convention
questions above.

When we get advol access:
- Add an `advol` backend to `render_voxel_reference.py` (~1 hour). Asset config
  table and `compare_renders.py` stay as-is.
- The Mitsuba path stays as a cross-check; not deleted.

## Course correction (today)

Going back to the previous strategy: compare against `assets/cloud/refs_pyr0/0000.exr`
directly and sweep `--sigma-multiplier`. Capped the cloud test to one frame
(`test/scenes/cloud_validation.cpp:47`: `MAX_CAMERAS = 1`) for fast iteration.

### Lesson: linear RMSE is the wrong metric for this problem

First sweep (linear RMSE against the full image) picked sigma=310. Visually
that render looked far too black — the cloud interior was clipped to solid
black. The metric was wrong, not the renderer. Why:

- Reference's deepest darks are `lum ~ 0.05-0.3`, not zero.
- Once our render's cloud interior saturates to 0, **further increases in sigma
  don't change anything inside the cloud** (already black). Linear RMSE sees
  saturated-black as "close enough" to ref's dark-but-not-zero darks.
- L2 over the full image is dominated by the bright background (which everyone
  gets right) — the metric reads 0 squared-error there and washes out the
  cloud-interior loss-of-gradient.

Per-pixel breakdown at the two candidates (512 SPP, ref vs render):

| metric | sigma=175 (visual pick) | sigma=310 (linear-RMSE pick) |
|---|---|---|
| Linear RMSE (full image) | 0.085 | **0.050** |
| Linear RMSE (within cloud silhouette) | 0.110 | 0.064 |
| **Log RMSE (perceptual proxy)** | **0.39** | 1.59 |
| % pixels saturated `<0.05` | 26.2% | 36.5% |
| Ref's % saturated `<0.05` | 24.9% | 24.9% |

sigma=310 has 50% more saturated-black pixels than the ref — the cloud is
over-extinct, just well-aligned with the ref's darks so L2 doesn't complain.

### Second sweep with log RMSE + saturation-match

Re-running with `logRMSE = sqrt(mean((log(ref) - log(test))^2))` and tracking
the fraction of `<0.05` pixels (proxy for "saturated black"):

| sigma | logRMSE | saturated `<0.05` |
|---|---|---|
| 130 | 0.585 | 14.2% |
| 150 | 0.433 | 21.0% |
| **160** | **0.408** | 23.4% |
| 170 | 0.426 | 25.4% |
| 175 | 0.451 | 26.2% |
| 180 | 0.485 | 27.0% |
| 200 | 0.668 | 29.4% |

Reference saturated `<0.05`: **24.9%**. Best logRMSE: sigma=160. Best
saturation match: sigma=170. User's visual pick of sigma=175 sits at the
edge of the flat minimum.

### Recommendation

Use `--sigma-multiplier 170` for `cloud_asset_validation`. It's where the
perceptual metric is near-flat AND the saturation fraction matches the
reference, so the cloud's interior gradient is preserved.

(The user's visual judgment of "around 175 looks best" is consistent — the
metric and the eye agree once we stop letting linear-RMSE pick the answer.)

### Caveats from earlier work that still apply

- The 2.2 figure in `memory/project_cloud_calibration.md` was for old buggy
  `expf(sigma_t)` math (removed); not relevant to current linear-sigma code.
- Prior linear-sigma empirical sweep landed at sigma ~250 (RMSE ~0.058) — that
  was also a linear-RMSE pick and is suspect for the same reasons.

## What to do later (when advol arrives)

1. `tools/refs/.venv/bin/pip install -e <path-to-advol>`
2. Add `--backend advol` to `render_voxel_reference.py`; route the render call
   through advol's helpers using its per-asset config.
3. Regenerate `assets/cloud/refs_voxel_self/` from advol.
4. Diff `refs_voxel_self/` against `refs_pyr0/` — if they match, Jorge's
   provenance claim is confirmed and we have a trustworthy ground truth.
   If they don't, we have hard data to push back with.
5. Also diff our DSYG render against `refs_voxel_self/` — that becomes the real
   correctness test, decoupled from the mystery `refs_pyr0/`.

## Files to keep, files to leave alone

- **Keep**: everything in `tools/refs/`. The venv, the diff tool, and the
  render script are all useful. The script's Mitsuba backend just needs the
  axis/bbox conventions sorted before its output is trustworthy.
- **Don't delete** `assets/cloud/refs_voxel_self/` — even if the contents are
  wrong now, the directory layout and naming convention are correct, and we'll
  overwrite once we have the right backend.
