# cuda-volprim

A from-scratch **CUDA/OptiX volumetric path tracer for Gaussian kernel-mixture volumes** —
a GPU implementation of the rendering method of *Don't Splat Your Gaussians* (Condor et
al., ACM TOG 2025), built and validated against its Mitsuba 3 reference implementation
(`volumetric_primitives`; see the [DSYG paper](https://doi.org/10.1145/3687934)).

Instead of the reference's segment-by-segment march with per-segment root-finding, this
renderer collects all of a ray's primitives in a **single acceleration-structure
traversal** and samples the scattering distance in **closed form across overlaps** via
per-primitive analytic free flights and an argmin rule (analog decomposition tracking).
At equal image quality it renders **2.7× faster** than the reference's corrected fast
estimator on the environment-lit showcase, is firefly-free at production sample budgets,
and passes a validation suite ranging from closed-form single-Gaussian transmittance to
pixel-level cross-renderer agreement. Method, validation, and all measurements:
see the thesis (link below).

## Requirements

| Component | Version used |
|---|---|
| GPU | NVIDIA, compute ≥ 8.6 (RTX 3090 used throughout; SER results need Ada) |
| CUDA | 12.6 |
| OptiX | 9.0 |
| Driver | 580.95 |
| Compiler | C++20 (GCC/Clang), CMake ≥ 3.24, Ninja |

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target test_runner
```

Opt-in build variants — all default OFF; the production binary contains none of them:

| CMake option | Effect |
|---|---|
| `-DTHESIS_ENABLE_SER=ON` | Shader Execution Reordering: one reorder per bounce (Ada GPUs only) |
| `-DTHESIS_ICOSPHERE=ON -DTHESIS_ICOSPHERE_N=<l>` | tessellated icosphere shell instead of the analytic sphere (accuracy/perf A/B) |
| `-DTHESIS_ENABLE_FAST_ERF=ON` | fast-erf approximation variant |

Per-ray buffer capacities are compile-time defaults in `device/core/constants.cuh`
(`#define THESIS_MAX_ACTIVE_PRIMS` / `THESIS_HIT_BUFFER_CAPACITY`); size them per asset
with `scripts/tools/calibrate_caps.sh <asset>`, which measures demand in-render, edits
the defaults, and rebuilds (see `experiments/13-caps`). The `THESIS_` prefix is
historical — the project began as a thesis codebase.

## Run

```sh
# fetch the HDR environment maps once
bash scripts/tools/fetch_envmaps.sh

# showcase: the Disney cloud under a measured HDR environment
build/bin/Release/test_runner --scene cloud_asset_scattering --spp 256 --seed 0 \
    --sigma-multiplier 7.5

# any Gaussian PLY asset through the generic loader
SG_PLY=assets/models/cloud/root.primitives_pyr0.ply SG_RES=512 SG_ENV=white_constant \
SG_ALBEDO=0.9 build/bin/Release/test_runner --scene asset_validation --spp 64 \
    --sigma-multiplier 10 --seed 0
```

Selected flags:

| Flag | Meaning |
|---|---|
| `--scene <name>` | scene to render (`cloud_asset_scattering`, `asset_validation`, ...) |
| `--spp <n>` | samples per pixel |
| `--seed <n>` | RNG seed (scattering renders are deterministic per seed) |
| `--sigma-multiplier <x>` | global density scale applied to every primitive's extinction |
| `--ris` / `--ris-candidates <K>` | product-RIS direct lighting (default: MIS, two shadow rays) |
| `--rr-depth <n>` | Russian-roulette start depth (default 12) |
| `--measure-caps` | report per-ray buffer demand (max hits/ray, max point-overlap) |

Environment variables for the generic asset loader (`--scene asset_validation`):

| Variable | Meaning |
|---|---|
| `SG_PLY` | path to a Gaussian PLY asset |
| `SG_RES` | square image resolution (default 512) |
| `SG_ENV` | environment: `white_constant`, `meadow`, `studio` |
| `SG_ALBEDO` | scattering albedo override (0 = absorption-only) |
| `SG_VIEW` | camera axis: `negz` (default), `posz`, `posx`, `negx`, `posy`, `negy`, `diag` |
| `SG_DIST` / `SG_FOV` | camera distance (3.5) and vertical FOV (40°) |
| `SG_CAM` | restrict a multi-camera scene to one camera index |

Renders are written as EXR to `test_results/`.

## Reproducing the thesis results

Every figure and table maps to a directory under [`experiments/`](experiments/README.md),
each stating the claim, the exact commands, expected values with tolerances, and hardware
requirements. Timing experiments need a locked GPU operating point
(`scripts/campaign/lock_clocks.sh`); reference-side experiments need the Mitsuba stack
described in [`experiments/mitsuba-reference/`](experiments/mitsuba-reference/README.md).

During validation, this project diagnosed and corrected five issues in the reference's
fast (next-event) estimator; the fixes and their deterministic certification probes are
in `experiments/16-corrections`, and were submitted upstream.

## Repository layout

```
src/ include/        host application (renderer, OptiX pipeline, IO)
device/              GPU code: megakernel, sampling, kernels
test/                test_runner and scene definitions
scripts/             campaign runners, plotting, tooling
experiments/         thesis-claim reproduction map (start here)
assets/              small assets + fetch scripts for the rest
latex/               LaTeX sources of the thesis
```

## Citation

Thesis: *Efficient Volume Rendering Through Primitive-Based Kernel Mixture Volumes*
(University of Luxembourg / USI, 2026). PDF and citation entry: see `CITATION.cff`.

The rendering method is from: Condor et al., *Don't Splat Your Gaussians: Volumetric
Ray-Traced Primitives for Modeling and Rendering Scattering and Emissive Media*,
ACM TOG 44(1), 2025.

## Acknowledgements and asset licenses

- Jorge Condor (IDSIA/USI) — the DSYG method and reference implementation, and the
  suggestion to apply decomposition tracking to this representation.
- The Disney cloud asset derives from the [Walt Disney Animation Studios cloud
  dataset](https://disneyanimation.com/resources/clouds/) (CC-BY-SA 3.0).
- The bunny asset derives from the Stanford Bunny (Stanford 3D Scanning Repository).
- Environment maps from Poly Haven (CC0).
