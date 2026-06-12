# Cap Calibration by Measurement — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace offline cap estimation with in-renderer measurement (`--measure-caps`) plus a one-command calibration wrapper, and land the sub-entry clamp fix from the capfree campaign.

**Architecture:** Observation-only counters ride the normal render: the COLLECT anyhit counts every invocation into a per-ray `total_seen_` (cap-independent — the anyhit fires for every hit even when the buffer is full), and gated `atomicMax` hooks in `sample_scattering_event` record the launch-wide maxima of hits-per-ray and point-overlap into a 2-slot device buffer mirroring the existing `overflow_counter_` pattern. A host flag allocates the buffer and prints measured maxima + suggested caps; a bash wrapper turns that into sed → rebuild → verify.

**Tech stack:** CUDA/OptiX megakernel (single TU `device/device_program.cu`), CLI11 (`src/thesis/host/app/config.cpp`, `test/test_runner.cpp`), CMake, validation via `test_runner` + `/home/kacper/thesis/tools/refs/exr_diff.py` (venv at `/home/kacper/thesis/tools/refs/.venv`).

**Context (verified on this worktree, branch `feature/cap-calibration` off `main` @ `f62101a`):**
- Build green, 43/43 suite. Assets symlinked (`assets/models` → main checkout; `white_constant.hdr` linked).
- Spec: `docs/superpowers/specs/2026-06-12-cap-calibration-design.md` (committed `572e8ad`).
- Main-state facts the spec didn't know: `scripts/tools/` does NOT exist on main (the estimator lives on `feature/icosphere-gas`; comparison numbers come from that branch's `results/campaign/caps_per_asset.md`, cited below). Main's overflow readout is a SINGLE counter (`renderer.cpp:191-196,354-366`) that only warns on overflow — there is no positive "Cap check" line, so the wrapper asserts the ABSENCE of the warning. `report_overflow()` takes no argument (`device/core/launch_params.cuh:13`).
- Per-asset estimator suggestions for the comparison table (from `caps_per_asset.md`, icosphere branch): cloud 128/128 (stock ok), tornado 112/432, explosion 32/176, bunny 320/496; raw estimator maxima where cited: explosion hit_max 136, bunny active_max 245.
- Flag-flow pattern to mirror exactly: `--ris` → `TestConfig.use_ris` (`test/test_runner.cpp:55,125`) → `renderer_config.use_ris_` (`test_runner.cpp:175,258`) → `rp.use_ris_` (`renderer.cpp:207`); standalone CLI in `config.cpp` Render-parameters group (~line 42).

**Rules:** every commit message imperative, truthful, no AI mentions. Nothing merges to main without the user's explicit acceptance (spec §7).

---

### Task 1: Sub-entry clamp in the buffered hit loop

**Files:**
- Modify: `device/core/sampling.cuh` (~line 395, the hit loop's `inv_cdf_segment` call)

- [ ] **Step 1:** Read `device/core/sampling.cuh:385-415`. The hit loop currently reads:

```cpp
        const float tau_j = sample_free_flight_tau(rng);
        ...
        const float t_scatter = prim.inv_cdf_segment(ray, hit_t, tau_j);
```

Change the `const float t_scatter` to `float t_scatter` and insert directly after it:

```cpp
        // Clamp FP undershoot: the true segment-CDF inverse is >= hit_t by construction
        // (optical depth from hit_t to hit_t is 0), but the erf/erfinv round-trip can
        // land a few ULPs below it (χ≈0 → τ≈0 → true solution == hit_t exactly). A
        // sub-entry winner is then EXCLUDED from its own scatter's active set by the
        // rebuild filter below (`hit_t > t_scatter_min → skip`), zeroing the albedo and
        // silently killing the path. Mechanism proven at bit level during the cap-free
        // streaming campaign (capfree_b_gate.md, branch feature/cap-free-streaming).
        // NaN/±inf saturation values fail the `>= 0` guard below and stay rejected.
        if (t_scatter >= 0.0f && t_scatter < hit_t) {
            t_scatter = hit_t;
        }
```

- [ ] **Step 2:** Build: `cmake --build build -j"$(nproc)"`. Expected: clean.

- [ ] **Step 3:** Furnace + suite:

```bash
SG_ALBEDO=1.0 SG_ENV=white_constant ./build/bin/Release/test_runner --scene single_gaussian_validation --spp 1024
/home/kacper/thesis/tools/refs/.venv/bin/python /home/kacper/thesis/tools/refs/furnace_check.py test_results/single_gaussian_validation/0000.exr
timeout 1200 ./build/bin/Release/test_runner --all 2>&1 | tail -4
```

Expected: furnace bias OK + structure OK; `Passed: 43, Failed: 0`.

- [ ] **Step 4:** Quantify the image footprint (fixed-seed cloud render, clamp vs pre-clamp): render now, then `git stash`, rebuild, render again, `git stash pop`, rebuild, diff:

```bash
SG_ENV=meadow SG_CAM=0 ./build/bin/Release/test_runner --scene cloud_asset_scattering --sigma-multiplier 7.5 --spp 16 --seed 42
cp test_results/cloud_asset_scattering/0000.exr /tmp/clamp_on.exr
git stash && cmake --build build -j"$(nproc)"
SG_ENV=meadow SG_CAM=0 ./build/bin/Release/test_runner --scene cloud_asset_scattering --sigma-multiplier 7.5 --spp 16 --seed 42
cp test_results/cloud_asset_scattering/0000.exr /tmp/clamp_off.exr
git stash pop && cmake --build build -j"$(nproc)"
/home/kacper/thesis/tools/refs/.venv/bin/python /home/kacper/thesis/tools/refs/exr_diff.py /tmp/clamp_on.exr /tmp/clamp_off.exr
```

Expected: ndiff between 0 and ~10 channel-values (the χ≈0 event rate at p≈2⁻³² predicts 0-or-few per image; the campaign's equivalent patch measured ≤6). Record the number for the commit message. If ndiff is LARGE (>100): STOP — that contradicts the mechanism; report BLOCKED.

- [ ] **Step 5:** Commit:

```bash
git add device/core/sampling.cuh
git commit -m "Clamp sub-entry inverse-CDF undershoot in the buffered hit loop

The erf/erfinv round-trip in inv_cdf_segment can land a few ULPs below its
hit_t lower bound (chi~0 draws). Such a winner is excluded from its own
scatter's active set by the rebuild filter, zeroing the albedo and silently
killing the path. Mechanism root-caused at bit level during the cap-free
streaming campaign (feature/cap-free-streaming, capfree_b_gate.md). Image
footprint at 16 spp cloud: <N> channel-values (seed 42); furnace + 43/43
suite unchanged."
```

(Replace `<N>` with the measured ndiff.)

---

### Task 2: Device-side measurement counters

**Files:**
- Modify: `device/core/hit_record.cuh` (HitBufferSoA)
- Modify: `device/entry/anyhit.cuh` (COLLECT branch)
- Modify: `include/thesis/common/params/launch_params.h` (RenderParams + LaunchParams)
- Modify: `device/core/launch_params.cuh` (measurement slot constants)
- Modify: `device/core/sampling.cuh` (two gated hooks)

- [ ] **Step 1:** `device/core/hit_record.cuh` — add to `HitBufferSoA` after `size_`:

```cpp
    // Total COLLECT-anyhit invocations this trace — counts EVERY entry hit, including
    // those dropped on overflow, so --measure-caps can read true per-ray demand from
    // any binary regardless of its compiled cap (the anyhit always fires; only the
    // push is capped). One local-memory add per hit; observation-only.
    uint32_t total_seen_ = 0;
```

and in `clear()` add `total_seen_ = 0;`.

- [ ] **Step 2:** `device/entry/anyhit.cuh` — in the COLLECT branch, immediately after the `auto* hit_buffer = unpack_ptr<HitBuffer>(...)` line, add:

```cpp
    ++hit_buffer->total_seen_;  // true hit count, cap-independent (see hit_record.cuh)
```

(Read the file first; main's COLLECT branch is the block doing `hit_buffer->push(...)` / `report_overflow()`.)

- [ ] **Step 3:** `include/thesis/common/params/launch_params.h`:
  - In `RenderParams`, after `ris_num_candidates_`:

```cpp
    bool measure_caps_ = false;           // --measure-caps: record launch-wide maxima of
                                          // hits/ray and point-overlap into measure_buf_
```

  - In `LaunchParams`, after `overflow_counter_`:

```cpp
    // Device-side maxima for --measure-caps ([0] = max COLLECT hits per ray,
    // [1] = max point-overlap at a path vertex). Null unless measurement is on;
    // written via atomicMax under the render_.measure_caps_ gate. Observation-only:
    // allocating/reading it never perturbs the render (bit-identical images).
    uint32_t* measure_buf_ = nullptr;
```

- [ ] **Step 4:** `device/core/launch_params.cuh` — after `report_overflow`, add:

```cpp
// Slot indices in LaunchParams::measure_buf_ (--measure-caps maxima).
inline constexpr int MEASURE_HIT_MAX = 0;     // max COLLECT-anyhit invocations per ray
inline constexpr int MEASURE_ACTIVE_MAX = 1;  // max point-overlap at a path vertex
```

- [ ] **Step 5:** `device/core/sampling.cuh` — two gated hooks in `sample_scattering_event` (read the function first; line refs are pre-Task-1 anchors):
  - **Hook A** (hits/ray): immediately after the `collect_hits(ray, hit_buffer, &miss);` call:

```cpp
    if (launch_params.render_.measure_caps_) {
        atomicMax(&launch_params.measure_buf_[MEASURE_HIT_MAX], hit_buffer.total_seen_);
    }
```

  - **Hook B** (point overlap): at the END of the scatter path, after `active_prims = final_active_prims;` (and before the `event.t_hit_ = ...` lines):

```cpp
    // --measure-caps: true point-overlap at the scatter vertex, counted by the same
    // containment predicate the bounce-0 scan uses — NOT final_active_prims.size(),
    // which is clipped by the compiled MAX_ACTIVE_PRIMS. O(N) per scatter, gated.
    if (launch_params.render_.measure_caps_) {
        uint32_t overlap = 0;
        for (size_t i = 0; i < num_primitives; ++i) {
            if (common::geometry::point_inside_bvh_bound(event.position_,
                                                         launch_params.primitives_[i]))
                ++overlap;
        }
        atomicMax(&launch_params.measure_buf_[MEASURE_ACTIVE_MAX], overlap);
    }
```

  - **Hook C** (camera-origin overlap, bounce 0): inside the `if (first_bounce)` scan block, count containment hits into a local `uint32_t origin_overlap = 0;` (increment next to the existing `point_inside_bvh_bound` success branch, regardless of insert success), and after the scan loop:

```cpp
        if (launch_params.render_.measure_caps_) {
            atomicMax(&launch_params.measure_buf_[MEASURE_ACTIVE_MAX], origin_overlap);
        }
```

  (`num_primitives` already exists in the function; `MEASURE_*` come from `core/launch_params.cuh`, already included.)

- [ ] **Step 6:** Build + suite: `cmake --build build -j"$(nproc)" && timeout 1200 ./build/bin/Release/test_runner --all 2>&1 | tail -4`. Expected: clean, `Passed: 43` (flag is false everywhere; `measure_buf_` is null and never dereferenced).

- [ ] **Step 7:** Commit:

```bash
git add device/core/hit_record.cuh device/entry/anyhit.cuh device/core/launch_params.cuh \
        device/core/sampling.cuh include/thesis/common/params/launch_params.h
git commit -m "Add observation-only cap-measurement counters to the device path

The COLLECT anyhit counts every invocation per ray (cap-independent: hits
beyond the buffer capacity still fire the anyhit), and gated atomicMax
hooks record launch-wide maxima of hits/ray and point-overlap into a
2-slot measure buffer, mirroring the overflow-counter pattern. Inactive
(null buffer, false flag) unless --measure-caps; render math untouched."
```

---

### Task 3: Host `--measure-caps` flag + readout

**Files:**
- Modify: `src/thesis/host/app/config.h` (find the Config struct; add field next to `use_ris_`)
- Modify: `src/thesis/host/app/config.cpp` (~line 42 Render-parameters group)
- Modify: `src/thesis/host/app/renderer.cpp` (buffer alloc ~line 191-196 pattern; readout ~line 354-366; member next to `overflow_counter_` — find the header where `overflow_counter_` is declared, likely `renderer.h` nearby, and mirror)
- Modify: `test/test_runner.cpp` (TestConfig field ~55, flag ~125, mapping at BOTH 175 and 258)

- [ ] **Step 1:** Add `bool measure_caps_ = false;` to `Config` (next to `use_ris_` — locate with `grep -n use_ris_ src/thesis/host/app/config.h`). In `config.cpp`'s Render-parameters group:

```cpp
    render_group->add_flag("--measure-caps", config.measure_caps_,
        "Measure max hits/ray and point-overlap during the render and print suggested caps");
```

- [ ] **Step 2:** `test/test_runner.cpp`: add `bool measure_caps = false;` to TestConfig (next to `use_ris`), the flag:

```cpp
    app.add_flag("--measure-caps", config.measure_caps,
                 "Measure max hits/ray and point-overlap; print suggested caps");
```

and `renderer_config.measure_caps_ = test_config.measure_caps;` at BOTH mapping sites (the lines that currently set `use_ris_`, ~175 and ~258).

- [ ] **Step 3:** `renderer.cpp` — mirror the overflow-counter lifecycle exactly:
  - Member: find where `overflow_counter_` is declared (`grep -rn "overflow_counter_" src/thesis include/thesis --include=*.h`) and add beside it: `cuda::AsyncBuffer<uint32_t> measure_buf_;`
  - Alloc (next to the overflow alloc, ~line 191):

```cpp
    // 2-slot maxima buffer for --measure-caps ([0]=hits/ray, [1]=point-overlap).
    // Allocated only when measuring; the device null-guards via render_.measure_caps_.
    if (config_.measure_caps_) {
        measure_buf_ = cuda::AsyncBuffer<uint32_t>(
            2, cuda_ctx_.get(), streams_[cuda::StreamKind::Main], cuda::AllocType::OnBoth);
        measure_buf_.memset_device(0);
        par.measure_buf_ = measure_buf_.device();
    }
    rp.measure_caps_ = config_.measure_caps_;
```

  (Put `rp.measure_caps_` with the other `rp.*` assignments ~line 203-209; keep the alloc with the buffers. Note `rp` is declared after the overflow alloc — order accordingly.)
  - Readout, after the overflow readback block (~line 366), same sync pattern:

```cpp
    if (config_.measure_caps_) {
        measure_buf_.download();
        streams_[cuda::StreamKind::Main]->synchronize();
        const auto hit_max = measure_buf_.host()[0];
        const auto active_max = measure_buf_.host()[1];
        const auto suggest = [](uint32_t v) {  // ceil(1.125*v) rounded up to multiple of 16
            const auto margined = (v * 9 + 7) / 8;
            return ((margined + 15) / 16) * 16;
        };
        spdlog::info("Cap measurement: max hits/ray = {}, max point-overlap = {}", hit_max,
                     active_max);
        spdlog::info("Suggested caps: HIT_BUFFER_CAPACITY = {}, MAX_ACTIVE_PRIMS = {}",
                     suggest(hit_max), suggest(active_max));
    }
```

- [ ] **Step 4:** Build + functional check:

```bash
cmake --build build -j"$(nproc)"
SG_ENV=meadow SG_CAM=0 ./build/bin/Release/test_runner --scene cloud_asset_scattering \
  --sigma-multiplier 7.5 --spp 16 --seed 42 --measure-caps 2>&1 | grep -E "Cap measurement|Suggested"
```

Expected: two log lines with plausible numbers (cloud: hits/ray ≤ ~128 — its stock cap never overflowed; point-overlap ≤ ~45 per the documented cloud measurement). Sanity: suggested caps ≤ stock 128/128 for the cloud.

- [ ] **Step 5:** **No-perturbation gate (bit-identical):**

```bash
cp test_results/cloud_asset_scattering/0000.exr /tmp/measure_on.exr
SG_ENV=meadow SG_CAM=0 ./build/bin/Release/test_runner --scene cloud_asset_scattering \
  --sigma-multiplier 7.5 --spp 16 --seed 42
/home/kacper/thesis/tools/refs/.venv/bin/python /home/kacper/thesis/tools/refs/exr_diff.py \
  test_results/cloud_asset_scattering/0000.exr /tmp/measure_on.exr
timeout 1200 ./build/bin/Release/test_runner --all 2>&1 | tail -4
```

Expected: `BIT-IDENTICAL`; `Passed: 43`. Any diff = the hooks perturb the render = bug; STOP and report.

- [ ] **Step 6:** Commit:

```bash
git add src/thesis/host/app src/thesis/host include/thesis test/test_runner.cpp
git commit -m "Add --measure-caps: in-render measurement of cap demand with suggestions

Allocates the 2-slot maxima buffer (overflow-counter pattern), threads the
flag Config->RenderParams->device, and prints measured maxima plus
suggested caps (1.125 margin, rounded up to 16). Measurement is
observation-only: a measured render is bit-identical to a normal one
(gated by exr_diff in validation); suite 43/43."
```

(Adjust the `git add` list to the files actually touched.)

---

### Task 4: `calibrate_caps.sh` wrapper + 4-asset calibration run

**Files:**
- Create: `scripts/tools/calibrate_caps.sh` (note: `scripts/tools/` is new on main — create it)
- Create: `results/campaign/cap_calibration.md`

- [ ] **Step 1:** Create `scripts/tools/calibrate_caps.sh` (mode `+x`):

```bash
#!/usr/bin/env bash
# One-command per-workload cap calibration (spec: 2026-06-12-cap-calibration-design.md).
# Measures true cap demand with --measure-caps on the asset's scattering stress
# (the binding workload, per caps_per_asset.md), writes the two constants, rebuilds,
# and verifies the calibrated build renders the stress with zero overflows.
# Usage: scripts/tools/calibrate_caps.sh <cloud|tornado|explosion|bunny> [spp] [seed...]
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

ASSET="${1:?usage: calibrate_caps.sh <asset> [spp] [seeds...]}"
SPP="${2:-16}"
shift $(( $# >= 2 ? 2 : 1 ))
SEEDS=("${@:-42 43}"); [ $# -eq 0 ] && SEEDS=(42 43)
BIN=build/bin/Release/test_runner
CONSTS=device/core/constants.cuh

run_measure() {  # $1 = seed → echoes "H A"
  local out
  case "$ASSET" in
    cloud)
      out=$(SG_ENV=meadow SG_CAM=0 $BIN --scene cloud_asset_scattering \
            --sigma-multiplier 7.5 --spp "$SPP" --seed "$1" --measure-caps 2>&1) ;;
    tornado|explosion|bunny)
      out=$(SG_PLY=assets/models/$ASSET/${ASSET}_pyr0.ply SG_ENV=meadow SG_ALBEDO=0.9 \
            SG_RES=512 SG_VIEW=diag $BIN --scene asset_validation \
            --spp "$SPP" --seed "$1" --measure-caps 2>&1) ;;
    *) echo "unknown asset: $ASSET" >&2; exit 2 ;;
  esac
  echo "$out" | sed -n 's/.*max hits\/ray = \([0-9]*\), max point-overlap = \([0-9]*\).*/\1 \2/p'
}

H_MAX=0; A_MAX=0
for S in "${SEEDS[@]}"; do
  read -r H A < <(run_measure "$S")
  [ -z "${H:-}" ] && { echo "FAIL: no measurement line (seed $S)"; exit 1; }
  echo "  seed $S: hits/ray=$H overlap=$A"
  (( H > H_MAX )) && H_MAX=$H; (( A > A_MAX )) && A_MAX=$A
done

suggest() { echo $(( ((($1 * 9 + 7) / 8 + 15) / 16) * 16 )); }
H_CAP=$(suggest "$H_MAX"); A_CAP=$(suggest "$A_MAX")
echo "Measured maxima: hits/ray=$H_MAX overlap=$A_MAX → caps HIT=$H_CAP ACTIVE=$A_CAP"

restore() { git checkout -- "$CONSTS"; }
trap restore ERR
sed -i "s/MAX_ACTIVE_PRIMS = [0-9]*;/MAX_ACTIVE_PRIMS = $A_CAP;/;s/HIT_BUFFER_CAPACITY = [0-9]*;/HIT_BUFFER_CAPACITY = $H_CAP;/" "$CONSTS"
cmake --build build -j"$(nproc)" >/dev/null

VERIFY_SEED=7   # deliberately UNMEASURED: validates the 1.125 margin
case "$ASSET" in
  cloud) VOUT=$(SG_ENV=meadow SG_CAM=0 $BIN --scene cloud_asset_scattering \
          --sigma-multiplier 7.5 --spp "$SPP" --seed $VERIFY_SEED 2>&1) ;;
  *)     VOUT=$(SG_PLY=assets/models/$ASSET/${ASSET}_pyr0.ply SG_ENV=meadow SG_ALBEDO=0.9 \
          SG_RES=512 SG_VIEW=diag $BIN --scene asset_validation --spp "$SPP" --seed $VERIFY_SEED 2>&1) ;;
esac
if echo "$VOUT" | grep -q "Cap overflow:"; then
  echo "VERIFY FAIL: calibrated caps overflowed on unmeasured seed $VERIFY_SEED"; restore; exit 1
fi
echo "VERIFY OK: $ASSET caps HIT_BUFFER_CAPACITY=$H_CAP MAX_ACTIVE_PRIMS=$A_CAP (0 overflows, seed $VERIFY_SEED)"
echo "NOTE: device/core/constants.cuh now holds the calibrated caps (not committed)."
```

- [ ] **Step 2:** Run it for all four assets, restoring stock constants between assets and recording every number:

```bash
for A in cloud tornado explosion bunny; do
  echo "=== $A ==="; scripts/tools/calibrate_caps.sh $A 16
  git checkout -- device/core/constants.cuh; cmake --build build -j"$(nproc)" >/dev/null
done
```

Expected per asset: two seed measurement lines, suggested caps, `VERIFY OK`. Cross-checks against the estimator (icosphere branch values): tornado measured hits should be ≤ 432-ish and ≥ stock-128-overflow levels (tornado at stock dropped 1.77M entries, so measured hits/ray MUST exceed 128); explosion marginal (~136 estimator hit_max — measured should land near/under); bunny hits large (≥ 320, estimator suggested 496), bunny overlap well under 245 (estimator's whole-bbox bound; renders never exceeded 128). If a VERIFY fails (margin too thin), bump the margin constant in BOTH the script and renderer.cpp `suggest` lambda (1.125 → 1.25), document, re-run.

- [ ] **Step 3:** Write `results/campaign/cap_calibration.md` (house style: Question/Method/Results/Conclusion): the measurement-vs-estimator table —

| asset | estimator caps (active/hit, icosphere branch) | measured max (overlap / hits-per-ray, seeds 42+43) | calibrated caps | verify (seed 7) |

— plus the spp/seed protocol, the margin formula, the no-perturbation gate result (Task 3), and the conclusion (per-workload measurement replaces the whole-bbox geometric estimate; estimator retained on its branch as the camera-independent ceiling; runtime overflow warning remains the safety net).

- [ ] **Step 4:** Final suite at stock constants (`git status` must show constants.cuh clean) + commit:

```bash
timeout 1200 ./build/bin/Release/test_runner --all 2>&1 | tail -4
git add scripts/tools/calibrate_caps.sh results/campaign/cap_calibration.md
git commit -m "Add one-command cap calibration; measure the 4-asset lineup

calibrate_caps.sh measures true per-workload cap demand via --measure-caps
(2 seeds), writes the constants, rebuilds, and verifies zero overflows on
an unmeasured seed. Measured demand vs the offline estimator's whole-bbox
bounds recorded in cap_calibration.md; the estimator stays (icosphere
branch) as the camera-independent ceiling."
```

---

### Task 5: Results package + user acceptance gate

- [ ] **Step 1:** Write a short closing section into `results/campaign/cap_calibration.md` (or a final paragraph): branch state (`git log --oneline main..HEAD`), what merging gives (clamp fix + measurement mode + wrapper), explicit note that nothing merges without user approval, and the cross-reference to the abandoned streaming branch's negative-result docs for the thesis.

- [ ] **Step 2:** Commit any remainder; report to the controller with: per-asset calibration table, all gate verdicts, commit list. The controller presents to the user for accept/merge/amend. **STOP — do not merge.**

---

## Self-review

- **Spec coverage:** §4.1 counters → Task 2; §4.2 flag/readout → Task 3; §4.3 wrapper → Task 4; §4.4 clamp → Task 1; §6 validation items 1-4 → Tasks 3.5, 1.3-1.4, 4.2, 4.3; §7 acceptance → Task 5. Spec's `caps_table.csv`/"Cap check" assumptions adapted to main's reality (documented in Context). ✓
- **Placeholders:** `<N>` in Task 1's commit message is deliberately measurement-filled; everything else concrete. ✓
- **Type consistency:** `measure_buf_` is `uint32_t*` device / `AsyncBuffer<uint32_t>` host; `atomicMax(uint32_t*, uint32_t)` is the unsigned-int CUDA overload — `uint32_t` aliases `unsigned int` on this platform (static_assert not needed; same convention as existing code). `total_seen_` uint32_t consistently. `MEASURE_*` indices used in Tasks 2/3 match. `suggest` formula identical in renderer.cpp and the script: ceil(9v/8) rounded up to 16. ✓
