# Per-workload cap calibration — measurement vs the offline estimator, 2026-06-12

**Question.** The icosphere branch sizes `MAX_ACTIVE_PRIMS` / `HIT_BUFFER_CAPACITY` from
`estimate_caps.py`, a whole-bbox Monte-Carlo bound (`results/campaign/caps_per_asset.md` on that
branch). Can *in-render measurement* (`--measure-caps`) replace it with tighter-but-sufficient
per-workload caps — measured on the actual binding stress, margined, and verified — in one command?

## Method

- **Counters (cap-independent).** `--measure-caps` adds two observation-only counters to the render:
  *hits/ray* is counted in the COLLECT anyhit, which counts **dropped** hits too, so the maximum is
  the true demand even when the current build's cap is exceeded; *point-overlap* is an O(N)
  containment scan over all primitives at each scatter point, **not** the clipped active set, so it
  too sees past the cap. Both are exact counts, not estimates.
- **No-perturbation gate (Task 3).** Renders with and without `--measure-caps` are bit-identical —
  the flag observes, never steers.
- **Workload.** The asset's **scattering stress** — the binding workload per `caps_per_asset.md`
  (cloud: `cloud_asset_scattering`, σ-multiplier 7.5, meadow; tornado/explosion/bunny:
  `asset_validation`, albedo 0.9, meadow, 512², diag view) at 16 spp, seeds **42 and 43**.
- **Margin.** caps = round-up-to-16(ceil(**1.125** · measured max)), applied per counter
  (`Suggested caps:` line in the renderer; same formula in the wrapper).
- **Verification.** `scripts/tools/calibrate_caps.sh` writes the two constants into
  `device/core/constants.cuh`, rebuilds, and re-renders the same stress at the deliberately
  **unmeasured seed 7** — pass = no `Cap overflow:` warning. Seed 7's RNG stream was never seen by
  the measurement, so a clean verify exercises the 1.125 margin, not the memorized maxima.
- **Clock-independence.** Run under the 150 W power cap — irrelevant here, since every number is a
  deterministic count over ray geometry, not a timing.

One command per asset:

```bash
scripts/tools/calibrate_caps.sh <cloud|tornado|explosion|bunny> [spp] [seeds...]
# defaults: spp 16, seeds 42 43; leaves constants.cuh holding the calibrated caps (uncommitted)
git checkout -- device/core/constants.cuh && cmake --build build -j   # restore stock 128/128
```

## Results

Per-seed measurements (16 spp):

| Asset | seed 42 (hits/ray, overlap) | seed 43 (hits/ray, overlap) |
|---|---|---|
| cloud | 85, 45 | 84, 45 |
| tornado | 329, 87 | 323, 86 |
| explosion | 132, 26 | 125, 25 |
| bunny | 464, 70 | 464, 71 |

Calibration vs the offline estimator (estimator caps from `caps_per_asset.md`, icosphere branch):

| Asset | Estimator caps (active / hit) | Measured maxima (overlap / hits-per-ray) | Calibrated caps (ACTIVE / HIT) | Verify (unmeasured seed 7) |
|---|---|---|---|---|
| cloud | stock-ok (128 / 128) | 45 / 85 | **64 / 96** | OK — no overflow |
| tornado | 112 / 432 | 87 / 329 | **112 / 384** | OK — no overflow |
| explosion | 32 / 176 | 26 / 132 | **32 / 160** | OK — no overflow |
| bunny | 320 / 496 | 71 / 464 | **80 / 528** | OK — no overflow |

Cross-checks against the stock-128 overflow record (all consistent):

- **tornado** measured 329 hits/ray ≫ 128 — explains the 1.77M dropped entries at stock.
- **bunny** measured 464 hits/ray — the 59M-drop case; its **overlap is only 71**, far under the
  estimator's whole-bbox active_max of 245, matching the icosphere finding that bunny's densest
  overlap point is never on a camera/scatter path (shell of 25 600 tiny Gaussians: rays *enter*
  hundreds of primitives, few overlap at a single *point*).
- **explosion** measured 132 hits/ray — 4 over stock 128, exactly the 4-drop "marginal" case, and
  under the estimator's raw hit_max of 136.
- **cloud sanity row:** measured point-overlap **45 == the documented 2σ overlap** in
  `device/core/constants.cuh` ("measured max overlap = 37 at 1σ, 45 at 2σ") — the new counter
  matches the historical 2σ figure (the counter itself tests the 3σ BVH bound — agreement here is consistency evidence, not the same criterion).

## Conclusion

**Yes — with one asymmetry worth recording.** All four calibrations verified clean on the unmeasured
seed, so per-workload measurement + 1.125 margin is *sufficient* on this lineup. Versus the
estimator:

- **Active caps: measurement is much tighter where the whole-bbox bound is conservative.** Bunny
  320 → **80** (4×) — the estimator bounds the densest point in the bbox, which renders never visit.
  Cloud 128 → 64. Tornado (112) and explosion (32) come out **identical** — there the estimator's
  bound is already tight.
- **Hit caps: tighter on three assets, *higher* on bunny.** Tornado 432 → 384, explosion 176 → 160,
  cloud 128 → 96. But bunny goes 496 → **528**: the measured worst chord (464 entry hits) exceeds
  the estimator's raw line max (387, recorded in its `caps_table.csv`) — the estimator's Monte-Carlo
  line sampling under-sampled bunny's worst chord by ~20 %, and only its 1.25 margin kept 496
  sufficient (387 × 1.25 → 496 > 464, hence the icosphere branch's clean verification). Measurement
  anchors the margin to observed demand instead of to a sampled estimate of it.
- **Workflow.** One command measures (2 seeds), writes the constants, rebuilds (~9 s), and verifies —
  no offline tooling in the loop. The calibrated caps are left in the working tree for the user to
  keep or discard; stock 128/128 stays canonical in git.
- **Safety net.** Caps remain compile-time, so a workload outside the calibrated envelope is caught
  by the always-on runtime `Cap overflow:` WARNING (overflow is *detectable*, then re-calibrate on
  that workload) — the same safe-not-silent contract as before.
- **The estimator is not obsolete.** It stays on the icosphere branch as the **camera-independent
  ceiling**: measurement is exact for *this* workload (scene + camera + integrator), the estimator
  bounds *any* camera. Size production builds by measurement; consult the estimator when the camera
  is unknown — with the bunny row as the caveat that its raw maxima are themselves Monte Carlo and
  rely on its 1.25 margin for slack.

## Closing the cap domain

**Branch state** (`feature/cap-calibration`, off `main` @ `f62101a`): `572e8ad` spec → `b4c92ca` plan →
`c89719f` sub-entry clamp (dead-path fix, footprint 3 channel-values at 9e-9) → `f34b00d` device
counters → `b53082f` `--measure-caps` (bit-identical no-perturbation gate) → `2b13379` wrapper +
this doc. Suite 43/43 at stock 128/128 throughout; nothing merges without explicit approval.

**Merging this branch gives:** the clamp correctness fix, the `--measure-caps` mode, and the
one-command `calibrate_caps.sh` workflow. The render path is otherwise byte-identical (clamp
footprint aside) — no algorithmic change.

**Cross-reference for the thesis:** the abandoned streaming alternative and its full evidence trail
(probe, bit-exact gates, four-hypothesis debugging cascade, perf verdict) live on
`feature/cap-free-streaming` under `results/campaign/capfree_*.md` — the negative result that
motivated keeping the buffered design and measuring its caps instead.

## Addendum 2026-06-13 — campaign re-measurement; bunny chord is FP-fragile

Re-ran `calibrate_caps.sh` for all four assets on the campaign branch (`feature/icosphere-gas`,
analytic mode) to stash the TUNED binaries for the Section-6 timing runs. Three reproduced the
2026-06-12 caps exactly: **cloud 64/96, tornado 112/384, explosion 32/160**. Bunny did **not**:

| run (branch) | bunny seed 42 | seed 43 | → HIT cap |
|---|---|---|---|
| 2026-06-12 (`main`/cap-calibration) | 464 | 464 | 528 |
| 2026-06-13 driver (`icosphere-gas`, build A) | ~435 | — | 496 |
| 2026-06-13 direct re-measure (build B) | 423 | 418 | (→480) |

Same seeds, same PLY, **stable overlap (71) but max hits/ray wobbling 418–464.** Cause: bunny's worst
chord is a near-grazing ray through the 25 600-Gaussian shell where ±1 entry hit flips under FMA
reorder across recompiles (the same grazing-sliver mechanism behind the icosphere N=3 reversal). So
bunny's hit demand is not merely under-sampled (the §"Conclusion" caveat) but **build-fragile**.

**Decision:** bunny stays at the conservative **80/528** — it bounds the worst value observed on any
build (464) with margin; the tighter per-build values (480/496) would risk a silent overflow drop on a
different build. Tornado/explosion/cloud are stable, so their measured caps stand. The always-on
`Cap overflow:` warning remains the backstop if a campaign workload exceeds 528.
