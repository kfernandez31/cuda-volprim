# Experiments — reproducing the thesis results

Every figure and table in the thesis maps to one directory below. Each directory's
README states: the claim it reproduces, the exact commands, the expected numbers with
tolerances, the runtime, and what hardware/software it needs.

## Requirement classes

| Class | Meaning |
|---|---|
| `[gpu]` | any CUDA GPU (compute ≥ 8.6 tested); correctness results are deterministic or seed-pinned |
| `[clocks]` | timing result — requires a locked operating point (350 W power limit, SM clock pinned; see `scripts/campaign/lock_clocks.sh`) |
| `[ada]` | requires an Ada-generation GPU (Shader Execution Reordering) |
| `[mitsuba]` | requires the Mitsuba 3 reference side: Condor's `volumetric_primitives` (revision as released with DSYG) and/or the corrected fork — see `mitsuba-reference/README.md` |
| `[historical]` | development-time measurement documented but not re-scripted (stated as such in the thesis) |

## Map: thesis figure/table → experiment

| Thesis | Experiment | Class |
|---|---|---|
| Fig 2.1 (MC integration) | `scripts/plots/mc_integ.py` (self-contained numpy) | — |
| Fig 5.1 (voxel-grid cross-check) | `01-voxel-crosscheck` | `[gpu]` `[mitsuba]` |
| Fig 5.2 (absorption stages) | `02-absorption-stages` | `[gpu]` `[mitsuba]` |
| Fig 5.3 (furnace, four arms) | `03-furnace` | `[gpu]` `[mitsuba]` |
| Fig 5.4 (convergence −1/2 slope) | `04-convergence` | `[gpu]` `[mitsuba]` |
| Fig 5.5 (pixel-level agreement) | `05-agreement-panels` | `[gpu]` `[mitsuba]` |
| Tab 5.1 (feature gates) | `06-feature-gates` | `[gpu]` `[mitsuba]` |
| Fig 5.6 (showcase) | `07-showcase` | `[gpu]` |
| Fig 6.1 (roofline) | `08-roofline` | `[gpu]` (Nsight Compute) |
| Fig 6.2 (Russian-roulette depth) | `09-rr-depth` | `[gpu]` `[clocks]` |
| Figs 6.3/6.4 (product-RIS) | `10-ris` | `[gpu]` `[clocks]` |
| Tab 6.4 / Fig 6.5 (icosphere shell) | `11-icosphere` | `[gpu]` `[clocks]` |
| Tabs 6.5/6.6 (SER) | `12-ser` | `[ada]` `[clocks]` |
| Tab 6.2 (development-time wins) | — | `[historical]` (provenance in the table's caption) |
| Tab 6.7 (cap estimates) | `13-caps` | `[gpu]` |
| Fig 7.1 / headline 2.72× | `14-likeforlike` | `[gpu]` `[clocks]` `[mitsuba]` |
| §7.1 sampler-only 2.87× (flat env) | `15-sampler-only` | `[gpu]` `[clocks]` `[mitsuba]` |
| Fig 7.2 / §7.2 corrections + Appendix A | `16-corrections` | `[mitsuba]` (probes are parameter-free) |
| Tab 7.3 (device memory) | `17-memory` | `[gpu]` |
| §7.3 (startup latency) | `18-startup` | `[gpu]` `[mitsuba]` |
| Fig 7.3 (generalisation) | `19-generalisation` | `[gpu]` `[mitsuba]` |
| Fig 7.4 / Tab 7.4 (scaling) | `20-scaling` | `[gpu]` `[clocks]` |
| §7.6/§8.3 bounce-0 scan fix | `21-bounce0-precompute` | `[gpu]` `[clocks]` (branch `feature/bounce0-camera-set`) |

## Prerequisites for the timing runners

Several timing runners consume prebuilt binary pairs stashed in `~/winbins`
(per-asset calibrated, analog, safe-512 variants). Build them first with the recipes in
`scripts/campaign/build_*.sh` — each edits the cap defaults, builds, stashes, and
restores the tree. Plot scripts default to `results/campaign/...` CSV paths; the
experiment runs regenerate those files (pass `--csv` to point elsewhere).

## Ground rules

- **Determinism:** absorption renders are deterministic; scattering renders are pinned by
  `--seed`. Expected values are quoted with the tolerance class the thesis uses
  (exact / within Monte-Carlo noise / bootstrap CI).
- **Timing:** wall-clock numbers reproduce only at the locked operating point; equal-quality
  *ratios* are robust to the operating point (thesis §5.1).
- **Assets:** small assets ship in `assets/`; large ones are fetched by
  `scripts/tools/fetch_envmaps.sh` and converted by the scripts in `mitsuba-reference/`.
- **Reference side:** `[mitsuba]` experiments need the DSYG reference; pin and setup are in
  `mitsuba-reference/README.md`. The corrected-reference arms use the fix PR
  (the corrected fork; fixes submitted to the authors — thesis §7.2).
