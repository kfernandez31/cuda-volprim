# Review prompt — MSc thesis full review (for a fresh Claude, max mode + subagents)

You are reviewing a complete MSc thesis draft. Be a rigorous, skeptical examiner, not a cheerleader.
The author has deliberately de-hyped this thesis and values **honest, non-overclaiming framing** above
all — flag any sentence that overstates, hand-waves, or claims more than the evidence supports, but do
NOT push them to add hype. Equally, flag genuine weaknesses, gaps, and unsupported claims.

## What the thesis is
A CUDA/OptiX physically based **volumetric path tracer for Gaussian kernel-mixture volumes** — a
from-scratch reimplementation and performance study of *Don't Splat Your Gaussians* (DSYG). Core
architectural contributions: (1) **single-trace any-hit collection** (gather every primitive a ray
enters in one BVH traversal, analytic exits), and (2) **analog-decomposition (argmin) scatter sampling**
(each collected primitive draws an independent closed-form free flight; the nearest wins — no
segment-marching, no root-finding). Reference for correctness/perf: Jorge Condor's Mitsuba `volprim`
implementation on the same RTX 3090.

## How to build and read
- Source: `thesis/latex/` — `latexmk -pdf thesis.tex` (should be 0 errors, 0 undefined refs, ~69 pp).
- Chapters: `thesis/latex/chapters/0{1..8}-*.tex`, plus `abstract.tex`. PDF: `thesis/latex/thesis.pdf`.
- Read the built PDF for flow + figures; read the `.tex` for exact wording + `file:line` in findings.

## Reproducing experiments (you are on this machine — verify numbers, don't just read them)
The most valuable thing you can do is **re-run the load-bearing measurements and confirm they match the
thesis**, and **cross-check the algorithm descriptions against the actual source** (`device/`, `test/`,
`src/`). Every thesis number traces to a record in `results/campaign/*.md` — verify against those and
re-run where you doubt one.

- **Our renderer (OptiX/CUDA).** Prebuilt, per-asset cap-calibrated binaries in `~/winbins/`:
  `exe_{cloud,tornado,explosion,bunny}` (calibrated caps), `exe_stock` (default 128/128), `exe_safe512`
  (512/512), `exe_analog` (pure-analog, NEE off). Activate one:
  `cp ~/winbins/exe_<x> build/bin/Release/test_runner && cp ~/winbins/ir_<x> build/device_program.optixir`.
  Render: `build/bin/Release/test_runner --scene <S> [--sigma-multiplier σ] [--spp N] [--seed K]
  [--width W --height H] [--phase-g 0.85] [--ris] [--denoise]`. Scenes: `single_gaussian_validation`,
  `two_gaussian_validation`, `cluster_validation`, `cloud_asset_validation` (absorption),
  `cloud_asset_scattering`, `asset_validation` (generic; set `SG_PLY=<ply>`). Env knobs:
  `SG_ENV={meadow,white_constant,studio}`, `SG_CAM=<idx>`, `SG_ALBEDO`, `SG_RES`, `SG_VIEW=diag`.
  Output: `test_results/<scene>/0000.exr` (`…_denoised.exr` with `--denoise`). Rebuild:
  `cmake --build build --target test_runner -j`.
- **Mitsuba reference baseline.** `tools/refs/with_jorge_mitsuba.sh tools/refs/.venv/bin/python
  tools/refs/<script>.py`, driven by env (`SG_ENV, SG_ALBEDO, SG_SIGMA, SG_SPP, SG_SEED, SG_HG_G,
  SG_NEE`). Scripts: `render_cloud_prb_absorption.py`, `render_asset_via_prb.py`,
  `render_{single_gaussian,two_gaussian,cluster}_via_prb.py`, `single_gaussian_analytic.py`. `SG_NEE=0`
  = analog (the unbiased mode used as the scattering ground truth). Venv `tools/refs/.venv` (mitsuba
  3.6.4 + volprim, `cuda_ad_rgb`).
- **Experiment drivers + records.** `scripts/campaign/run_*.sh` reproduce specific experiments
  (`run_g1*.sh` → the 59× headline + the +156% NEE bias; `run_g3_scaling.sh` → scaling; `run_g1_flat.sh`
  → the flat rung); recorded in `results/campaign/*.md` (`g1_headline.md`, `scaling.md`, `g1_flat.md`,
  `vram.md`, ...). Check the thesis against these; re-run a driver to confirm one from scratch.
- **Equal-quality metric (to recompute speedups):** k = per-pixel inter-seed variance (ddof=1) × spp;
  clipped variant clips at the 99.9th percentile (discounts sparse fireflies); equal-quality speedup =
  (k·t)_reference / (k·t)_ours, over independent seeds of the same scene (load EXRs via OpenEXR + numpy).
- **Timing caveat (important):** faithful *timing* needs the GPU pinned — SM 1800 / mem 9751 / 350 W via
  `sudo bash scripts/campaign/lock_clocks.sh` (needs sudo; **ask the user — power is currently capped at
  150 W**, which throttles under load and corrupts frame times). Variance, RMSE, mean, and correctness
  are **clock-independent** — verify those freely; treat absolute frame times as reproducible only under
  the lock.
- **Key source for claim-verification:** `device/core/sampling.cuh` (argmin free-flight + inverse-CDF),
  `include/thesis/device/params/primitive.h` (Gaussian density + normalisation convention),
  `device/entry/raygen.cuh` (single-trace collection, path loop, NEE/MIS),
  `src/thesis/host/utils/io/ply.cpp` (σ_t / mass convention vs Mitsuba), `device/core/constants.cuh`
  (caps + `ENABLE_NEE`/`ENABLE_ANALYTIC_DIRECT` flags). Confirm Ch4's algorithm and §4.3's density
  convention match these.
- **Do NOT** `git push`, merge to main, delete branches, or `git gc/prune/reflog expire` — author's
  actions only. Committing review *notes* is fine if asked.

## Load-bearing claims to scrutinize hardest (verify support, and REPRODUCE where you can)
1. **The ~59× equal-quality headline** (Ch7 `sec:results-perf`): ours-MIS vs Mitsuba-analog on the
   env-lit cloud, clipped per-pixel variance at equal time. Is the framing honest? It rests on
   Mitsuba's NEE being +156% energy-biased on this medium (so its *only* unbiased mode is the
   firefly-noisy analog one). Check that the bias claim (Ch7 `sec:results-firefly`) and the
   "59× is environment importance sampling" scoping (the flat-env rung) are consistent and fair.
2. **The flat-env rung** (Ch7): on a flat env, ours-analog is ~3× faster/sample but ~5× higher
   variance (net ~0.6×); the 59× is env-IS-specific. Is this honestly presented (it partly deflates
   the headline — confirm it reads as honest scoping, not buried)?
3. **The scaling study** (Ch7 `sec:results-scaling`): the claim t∝N^0.40 rests on a *synthetic*
   square-grid sweep; the 4 production assets are a cost table, NOT a scaling curve (because N is
   confounded with packing density). Is this distinction clearly and correctly drawn?
4. **Validation methodology** (Ch5): the differential approach (structured difference = bug;
   unstructured/converging = MC noise), the absorption ladder (vs analytic + Mitsuba), the scattering
   unbiasedness (furnace energy conservation + mean convergence, `fig:scattering-ladder`). Is the
   evidence sufficient for the unbiasedness claims? Is the "~10^-4 unbiased" claim supported?
5. **Memory win** (Ch7 `sec:results-memory`): 578 < 838 MiB vs Mitsuba, and that per-asset cap
   calibration is what secures it. Check the argument that per-ray reservation (not geometry) dominates.
6. **The negative-results autopsy** (Ch6 `sec:autopsies`), incl. the folded-in A1 (collision- vs
   track-length-estimator) entry. Are the "lessons" justified by the measurements quoted?
7. **The architecture chapter** (Ch4): does the argmin/ADT description match a correct algorithm? Any
   hand-waving in the any-hit collection or the analytic-exit reasoning?

## Review dimensions (decompose across subagents)
- **A. Technical correctness:** algorithms (Ch4), validation logic (Ch5), every quantitative claim and
  whether the cited evidence supports it. Flag unsupported or internally inconsistent numbers.
- **B. Honesty / overclaiming:** every superlative, every "X×", every causal claim. Is it hedged
  appropriately? (The author wants this tight.)
- **C. Consistency:** numbers/terminology/notation across chapters (e.g., does the 59× appear
  consistently? caps, σ_t convention, asset names, primitive counts).
- **D. Figures & captions:** does each figure support its caption and the surrounding text? Are axes,
  units, and metrics defined? Any figure that doesn't earn its place?
- **E. Writing & structure:** clarity, flow, redundancy, signposting; abstract vs body alignment;
  whether Ch1 contributions match what's delivered.
- **F. Related work & positioning (Ch2/Ch3):** is DSYG and the broader volumetric-rendering context
  fairly and adequately covered? Any missing comparison a committee would expect?
- **G. Gaps a committee would raise:** what's the most likely tough question at the defense, and does
  the thesis pre-empt it?

## Known, deliberate decisions (do NOT flag as omissions — but do sanity-check the framing)
- **Voxel-grid cross-check: absorption is IN, scattering is intentionally OUT.** Ch5 keeps the clean
  *absorption* dense-grid cross-check (`sec:voxel-gt`, `fig:voxel-gt`). The *scattering* dense-grid GT
  was investigated and dropped (see `results/campaign/voxgrid_DECISION.md`): clean + unbiased +
  tractable is fundamentally infeasible for this high-dynamic-range cloud, so scattering validation
  rests on the furnace invariant + Mitsuba-analog differential + mean convergence. Check that's
  *adequate* — don't flag the scattering voxel GT as "missing." (No AdVol mention belongs in the thesis.)
- **Ch4 derivations + diagrams are present and intended** — the closed-form optical-depth derivation
  (`sec:analytic-od`), the argmin-exactness proof (`sec:adt`), and three TikZ schematics (`fig:argmin`,
  `fig:optical-depth`, `fig:per-ray-state`). Verify the derivations are correct and the figures match
  the text; they are not placeholders.
- The analytic built-in sphere (not the tessellated icosphere) is the operating point everywhere except
  the icosphere comparison in Ch6.
- The core-sampler equal-quality number is deliberately *not* given a single value under env lighting
  (it's firefly-metric-unstable there); the stable number is the flat-env one.

## Output format
Return findings grouped by severity:
1. **Blocking** — wrong/unsupported claims, incorrect algorithm descriptions, internal contradictions.
2. **Should-fix** — overclaims, unclear arguments, weak evidence, figure/caption mismatches.
3. **Polish** — wording, redundancy, typos.
Each finding: `chapter file:line` (or figure/section), the issue in one sentence, and a concrete fix.
Prioritise the load-bearing claims above. Be specific and quote the offending text.
