# Section-6 Window Campaign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce every remaining quantitative number and figure for thesis Chapters 6–7 (G1 headline, G2 ladder, G3 flat/studio rungs, G4 bunny profile, G6 confirms, plus prep), at the **calibrated per-asset caps**, under the locked-clock window.

**Architecture:** Each experiment = render sweep → k-extraction (inter-seed variance) → CSV + markdown record under `results/campaign/` → one commit. Timing always interleaved-within-block; correctness gates before any timed use of a new binary. Calibrated caps (the TUNED baseline) come from `scripts/tools/calibrate_caps.sh`, not the old estimator.

**Tech Stack:** CUDA/OptiX renderer (`test_runner`), Mitsuba 3.6.4 via `tools/refs/.venv-volprim`, Python analysis via `tools/refs/.venv`, `latexmk` for the thesis, `ncu` for profiles.

---

## Context you must load first (10 min, no exceptions)

1. `docs/superpowers/specs/2026-06-10-section6-experiments.md` — the runbook. G5/G7/G8 are DONE; G3-meadow and the RR sweep are DONE; §0.1–0.3 DONE. Status blocks inline.
2. `results/campaign/*.md` — every banked record. Especially `cap_calibration.md` (the caps), `ris_ksweep.md`, `rr_depth.md`, `caps_ab.md` (the GPU-contention lesson).
3. `docs/superpowers/HANDOFF.md` §"EXPERIMENTS HANDOFF" — gotchas (read the "Hard-won gotchas" list TWICE).

**State you inherit:** branch `feature/icosphere-gas` @ `c2e321c`+ (contains: icosphere A/B, RR+K sweeps, `main`'s cap-calibration + `--measure-caps` + sub-entry clamp, all thesis updates through Ch 6). Canonical `constants.cuh` = stock 128/128. Thesis builds clean at 59 pp.

**Calibrated caps (sizing authority = measurement, `cap_calibration.md`):**

| asset | ACTIVE/HIT | PLY |
|---|---|---|
| cloud | 64/96 | `assets/models/cloud/root.primitives_pyr0.ply` (loads via `--scene cloud_asset_*`) |
| tornado | 112/384 | `assets/models/tornado/tornado_pyr0.ply` |
| explosion | 32/160 | `assets/models/explosion/explosion_pyr0.ply` |
| bunny | 80/528 | `assets/models/bunny/bunny_pyr0.ply` |

(If the tornado/explosion PLYs are missing — they are gitignored — regenerate:
`tools/refs/.venv/bin/python tools/refs/npy_asset_to_ply.py assets/models/unpacked/<a>/optimized_asset_pyr0/npy_data assets/models/<a>/<a>_pyr0.ply`)

## Global rules (violations have burned previous sessions)

- **R1 — Power/clocks:** timing tasks REQUIRE ≥300 W + locked clocks (Task 0). At 150 W, run only count/correctness/ncu tasks. Never compare timings across sessions; interleave arms within blocks.
- **R2 — No `/tmp` for artifacts.** `/tmp` was cleaned mid-session twice. Per-seed EXRs → `results/campaign/<name>_seeds*/` (gitignore each dir). Stashed binaries → `~/winbins/`.
- **R3 — Binary-swap technique:** `OPTIXIR_PATH` is baked absolute → a stashed build is the *pair* (exe + `build/device_program.optixir`). Stash and restore both. Never rebuild `build/` while a sweep is running from it (copy the exe out first; the optixir must stay in place and untouched).
- **R4 — Canonical restore:** after any per-asset build: `git checkout device/core/constants.cuh && cmake --build build --target test_runner -j`. Verify `git status --short device/core/constants.cuh` is clean before every commit.
- **R5 — Asset scenes ignore `--width/--height`** (cloud scenes are 900×600 native; `asset_validation` uses `SG_RES`). Env/camera via env vars: `SG_ENV={meadow|studio|<unset>=white}`, `SG_CAM=0`, `SG_PLY`, `SG_VIEW`, `SG_ALBEDO`, `SG_RES`.
- **R6 — Commits:** one per banked result, NO AI mentions, message format per CLAUDE.md. `git push` / branch deletion / merge-to-main are Kacper's. `git gc`/`prune` are forbidden (orphan-recovered `deprecated-*` branches).
- **R7 — Frozen protocol for every k measurement:** 64 spp pinned, seeds 1–16, arm-interleaved within seed block, `SG_CAM=0`, k = per-pixel inter-seed variance (ddof=1), mean over pixels+channels, × spp. Equal-quality speedup = (k·t)_baseline / (k·t)_arm. Bootstrap CIs over seeds (50 resamples).
- **R8 — k transfer rule:** cap-size changes with zero overflows are bit-identical-class (recompile FMA reorder only, max|Δ|≈7e-5, means equal — `caps_ab.md`). Therefore banked k values remain valid for calibrated-cap builds; only **timings** need re-measuring. Never re-render 16-seed sweeps just because caps changed.

---

## Execution strategy — 150 W prep vs 350 W window (added 2026-06-13)

The campaign splits cleanly by what actually needs the power window. **Builds (CPU `nvcc`/OptiX-IR), correctness/count gates, `ncu` (self-locks clocks), and huge-margin confirms are power-immune**; only absolute wall-clock timings need ≥300 W + a quiet GPU.

**Phase A — 150 W, runnable anytime (compile-dominated, headless):**
- Prep: `extract_k.py` (+regression test, reproduces banked d12=1.98417), `env_peakiness.py` (measured: white 1×/0.6%, studio 538.8×/39.7%, meadow 1.53e5×/78.1%), studio-env wiring — **DONE + committed 2026-06-13**.
- All builds, stashed to `~/winbins/` as exe+optixir **pairs** (rule R3): 4 calibrated assets + stock, the 4 ladder pairs + fast-erf + BARE (worktrees), wavefront ON/OFF, adaptive. (cloud calibrated pair + fast-erf `build-ferf/` **DONE 2026-06-13** via `scripts/campaign/build_prewindow.sh`.)
- Cap-immune experiments: G4 bunny ncu, G6 wavefront+adaptive, G10 Mitsuba parity gates, G5 VRAM.

**Phase B — ≥300 W + quiet GPU (pure timing):**
- Re-anchors (RR, meadow RIS — k banked per R8, time-only), G3 flat/studio rungs, G2 ladder A/B + fast-erf, G1 headline (the 3–5 h pole — its own long/overnight window).

**Window triage (2-h slots, e.g. Piotr's 11:30–13:30).** A 2-h slot can't hold Phase B (~5–6.5 h), so pack it with the *fast-timing bucket* and pre-build its inputs at 150 W beforehand. Fits in ~40 min core + buffer: **RR re-anchor + meadow RIS re-anchor + G3 flat rung + G3 studio rung** → completes `fig:ris-ksweep` and clears the RR/RIS "provisional" flags (fast-erf A/B optional if time, runs `build-ferf/` directly per R3). Driver `scripts/campaign/run_window.sh` aborts if power <300 W, has env-leak guards on the flat/studio rungs, and runs a clock sentinel. Does NOT fit, want their own long window: **G1 headline**, **G2 ladder A/B** (needs ~2–3 h of worktree builds first).

**Headless launch (survives SSH drop):**
```bash
setsid nohup bash scripts/campaign/<driver>.sh >/dev/null 2>&1 </dev/null &   # tees its own log + writes results/campaign/.<name>.status
tail -f results/campaign/<name>.log
```

---

### Task 0: Verify state + window preconditions

**Files:** none (verification only)

- [ ] **Step 1: Repo state.**
```bash
cd /home/kacper/thesis
git branch --show-current        # expect: feature/icosphere-gas
git status --short | grep -vE "^\?\?" ; echo "---"
grep -nE "MAX_ACTIVE_PRIMS = |HIT_BUFFER_CAPACITY = " device/core/constants.cuh
```
Expected: branch correct, no modified tracked files, caps `128`/`128`.

- [ ] **Step 2: Build + furnace gate the inherited binary.**
```bash
cmake --build build --target test_runner -j 2>&1 | tail -2
SG_ALBEDO=1.0 build/bin/Release/test_runner --scene single_gaussian_validation --spp 1024 2>&1 | grep "Cap check"
tools/refs/.venv/bin/python tools/refs/furnace_check.py test_results/single_gaussian_validation/0000.exr | grep "=>"
```
Expected: `Cap check: 0 overflows`, `=> PASS`.

- [ ] **Step 3: Window preconditions (ask Kacper — he must run these, root):**
Tell Kacper to run, via the `!` prefix:
```
! sudo nvidia-smi -pm 1
! sudo nvidia-smi -lgc 1800,1800
! sudo nvidia-smi -lmc 9751,9751
```
Then verify:
```bash
nvidia-smi --query-gpu=power.limit,clocks.sm,clocks.mem,persistence_mode --format=csv,noheader
who | awk '{print $1}' | sort -u ; nvidia-smi --query-compute-apps=pid --format=csv,noheader
```
Expected: `350.00 W, 1800 MHz, 9751 MHz, Enabled`; no foreign compute processes. Known reality: under load SM settles ~1605–1755 MHz (thermal; the lock caps boost, doesn't prevent pull-down) — fine, record it. If power stays 150 W: do Tasks 1–4, 10, 12 only; ask Kacper for the window before 5–9, 11, 13.

- [ ] **Step 4: Contention sentinel (run during every timed sweep).**
```bash
nvidia-smi --query-gpu=clocks.sm --format=csv,noheader,nounits -lms 200 > results/campaign/clk_$(date +%H%M).log &
# kill %1 when the sweep ends; report min/p5/p50/max in the record
```

---

### Task 1: Reusable k-extraction script

**Files:**
- Create: `scripts/tools/extract_k.py`
- Test against: `results/campaign/rr_seeds/` (banked; must reproduce k(depth 12) = 1.98417)

- [ ] **Step 1: Write the script.**
```python
#!/usr/bin/env python3
"""k-extraction for equal-quality campaigns (thesis k-convention: k = noise^2 * N).

Reads <dir>/<arm>_s<seed>.exr for each arm and seed, computes per-pixel inter-seed
variance (ddof=1), averages over pixels+channels, multiplies by spp -> k per arm.
With --times (CSV: arm,seed,time_s) also computes per-block-normalized median
relative time, eff = k * t_rel, and bootstrap CIs on eff_base/eff_arm.

Usage:
  tools/refs/.venv/bin/python scripts/tools/extract_k.py --dir results/campaign/rr_seeds \
      --arms d5 d6 d8 d10 d12 d16 --seeds 1 16 --spp 64 [--times .../times.csv --base d12]
"""
import argparse, csv, statistics as st
import numpy as np, OpenEXR, Imath

def load(p):
    f = OpenEXR.InputFile(p); dw = f.header()['dataWindow']
    w = dw.max.x - dw.min.x + 1; h = dw.max.y - dw.min.y + 1
    pt = Imath.PixelType(Imath.PixelType.FLOAT)
    return np.stack([np.frombuffer(f.channel(c, pt), dtype=np.float32).reshape(h, w)
                     for c in ('R', 'G', 'B')], -1)

ap = argparse.ArgumentParser()
ap.add_argument('--dir', required=True)
ap.add_argument('--arms', nargs='+', required=True)
ap.add_argument('--seeds', nargs=2, type=int, default=[1, 16])
ap.add_argument('--spp', type=int, required=True)
ap.add_argument('--times')          # optional CSV: arm,seed,time_s
ap.add_argument('--base')           # arm name for speedup denominator
ap.add_argument('--boot', type=int, default=50)
a = ap.parse_args()
seeds = list(range(a.seeds[0], a.seeds[1] + 1))

stacks = {arm: np.stack([load(f'{a.dir}/{arm}_s{s}.exr') for s in seeds]) for arm in a.arms}
k = {arm: float(stacks[arm].var(axis=0, ddof=1).mean()) * a.spp for arm in a.arms}

tmed, trel = {}, {}
if a.times:
    rows = [(r[0], int(r[1]), float(r[2])) for r in list(csv.reader(open(a.times)))[1:]]
    blocks = {}
    for arm, s, t in rows: blocks.setdefault(s, {})[arm] = t
    bm = {s: st.mean(b.values()) for s, b in blocks.items()}
    for arm in a.arms:
        tmed[arm] = st.median([blocks[s][arm] for s in seeds if arm in blocks.get(s, {})])
        trel[arm] = st.median([blocks[s][arm] / bm[s] for s in seeds if arm in blocks.get(s, {})])

print(f"{'arm':>8} {'k':>10}" + (f" {'t_med':>8} {'t_rel':>7} {'eff':>9}" if a.times else ""))
for arm in a.arms:
    line = f"{arm:>8} {k[arm]:>10.5f}"
    if a.times: line += f" {tmed[arm]:>8.3f} {trel[arm]:>7.4f} {k[arm]*trel[arm]:>9.5f}"
    print(line)

if a.times and a.base:
    rng = np.random.default_rng(0)
    print(f"\nspeedup vs {a.base} (eff ratio), bootstrap {a.boot} resamples:")
    for arm in a.arms:
        if arm == a.base: continue
        boots = []
        for _ in range(a.boot):
            idx = rng.integers(0, len(seeds), len(seeds))
            kb = float(stacks[a.base][idx].var(axis=0, ddof=1).mean()) * a.spp
            ka = float(stacks[arm][idx].var(axis=0, ddof=1).mean()) * a.spp
            tb = st.median([blocks[seeds[i]][a.base] for i in idx])
            ta = st.median([blocks[seeds[i]][arm] for i in idx])
            boots.append(kb * tb / (ka * ta))
        lo, hi = np.percentile(boots, [2.5, 97.5])
        kk = k[a.base] * tmed[a.base] / (k[arm] * tmed[arm])
        print(f"  {arm:>6}: {kk:.3f}  [{lo:.3f}, {hi:.3f}]")
```

- [ ] **Step 2: Regression test against the banked RR sweep.** The banked EXRs are named `d{depth}_s{seed}.exr`:
```bash
tools/refs/.venv/bin/python scripts/tools/extract_k.py \
  --dir results/campaign/rr_seeds --arms d5 d6 d8 d10 d12 d16 --seeds 1 16 --spp 64
```
Expected: `d12  1.98417` (±1e-4) and `d5 2.46115` — matches `rr_depth.csv`. If not, the script is wrong; do not proceed.

- [ ] **Step 3: Commit.**
```bash
git add scripts/tools/extract_k.py
git commit -m "Add reusable k-extraction script (reproduces banked RR-sweep k exactly)"
```

---

### Task 2: Wire the studio environment into the scene harness

**Files:**
- Modify: `test/scenes/cloud_validation.cpp:70-73` and `:128-131`

- [ ] **Step 1: Edit both SG_ENV mappings.** Both sites currently read:
```cpp
    scene.env_map_override = (sg_env && std::string_view(sg_env) == "meadow")
                                 ? "assets/environment_maps/meadow_2_4k.hdr"
                                 : "assets/environment_maps/white_constant.hdr";
```
Replace each with:
```cpp
    const std::string_view env_sel = sg_env ? std::string_view(sg_env) : std::string_view{};
    scene.env_map_override = env_sel == "meadow"
                                 ? "assets/environment_maps/meadow_2_4k.hdr"
                             : env_sel == "studio"
                                 ? "assets/environment_maps/ferndale_studio_01_4k.hdr"
                                 : "assets/environment_maps/white_constant.hdr";
```
(One of the two sites assigns through a slightly different lvalue — keep each site's lvalue, change only the selection expression.)

- [ ] **Step 2: Rebuild + sanity render.**
```bash
ls assets/environment_maps/ferndale_studio_01_4k.hdr   # must exist (else scripts/tools/fetch_envmaps.sh)
cmake --build build --target test_runner -j 2>&1 | tail -1
SG_ENV=studio SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering --spp 4 2>&1 \
  | grep -E "environment map|Cap check"
```
Expected: a "Successfully loaded environment map" line with 4k dimensions, `Cap check: 0 overflows`.

- [ ] **Step 3: Commit.**
```bash
git add test/scenes/cloud_validation.cpp
git commit -m "Wire SG_ENV=studio (ferndale_studio_01) into the cloud/asset scenes"
```

---

### Task 3: Peakiness script (fig:ris-ksweep x-axis)

**Files:**
- Create: `scripts/tools/env_peakiness.py`
- Create: `results/campaign/env_peakiness.csv`

- [ ] **Step 1: Write the script** (pure-python RGBE `.hdr` reader; sin-θ solid-angle weighting):
```python
#!/usr/bin/env python3
"""Peakiness metrics for equirect RADIANCE .hdr envs: max/mean luminance ratio and
the energy fraction held by the top 0.1% of solid angle. Both sin(theta)-weighted.
Usage: env_peakiness.py a.hdr b.hdr ... [--csv out.csv]"""
import argparse, sys
import numpy as np

def read_hdr(path):
    with open(path, 'rb') as f:
        if not f.readline().startswith(b'#?'): raise ValueError('not RADIANCE')
        while True:
            line = f.readline()
            if line in (b'\n', b'\r\n'): break
        dims = f.readline().split()                  # b'-Y' H b'+X' W
        h, w = int(dims[1]), int(dims[3])
        data = np.empty((h, w, 4), np.uint8)
        for y in range(h):
            head = f.read(4)
            if head[:2] == b'\x02\x02' and (head[2] << 8 | head[3]) == w:  # new-style RLE
                for c in range(4):
                    x = 0
                    while x < w:
                        n = f.read(1)[0]
                        if n > 128:                   # run
                            data[y, x:x + n - 128, c] = f.read(1)[0]; x += n - 128
                        else:                         # literal
                            data[y, x:x + n, c] = np.frombuffer(f.read(n), np.uint8); x += n
            else:                                     # flat scanline
                row = head + f.read(4 * w - 4)
                data[y] = np.frombuffer(row, np.uint8).reshape(w, 4)
    e = data[..., 3].astype(np.int32)
    scale = np.where(e == 0, 0.0, np.ldexp(1.0, e - 136))   # 2^(E-128-8)
    rgb = data[..., :3].astype(np.float64) * scale[..., None]
    return rgb

ap = argparse.ArgumentParser(); ap.add_argument('hdrs', nargs='+'); ap.add_argument('--csv')
a = ap.parse_args()
rows = []
for p in a.hdrs:
    rgb = read_hdr(p)
    lum = rgb @ [0.2126, 0.7152, 0.0722]
    h, w = lum.shape
    sa = np.sin((np.arange(h) + 0.5) / h * np.pi)[:, None] * np.ones((1, w))  # ∝ solid angle
    energy = (lum * sa).ravel(); saw = sa.ravel()
    mean = energy.sum() / saw.sum()
    peak = lum.max() / mean
    order = np.argsort(energy)[::-1]
    cut = np.searchsorted(np.cumsum(saw[order]), 0.001 * saw.sum())
    frac = energy[order][:cut + 1].sum() / energy.sum()
    rows.append((p.split('/')[-1], w, h, lum.max(), mean, peak, frac))
    print(f"{p}: {w}x{h}  max/mean = {peak:.4g}   top-0.1%-solid-angle energy = {100*frac:.1f}%")
if a.csv:
    import csv as _c
    with open(a.csv, 'w', newline='') as f:
        _c.writer(f).writerows([('env', 'w', 'h', 'max_lum', 'mean_lum', 'peak_ratio', 'top01pct_energy')] + rows)
```

- [ ] **Step 2: Run on the three envs.**
```bash
tools/refs/.venv/bin/python scripts/tools/env_peakiness.py \
  assets/environment_maps/white_constant.hdr \
  assets/environment_maps/ferndale_studio_01_4k.hdr \
  assets/environment_maps/meadow_2_4k.hdr --csv results/campaign/env_peakiness.csv
```
Expected ballparks: white ≈ 1×; studio ≈ 700× (~47 % top-0.1 %); meadow ≈ 2×10⁵ (~74 %). If RLE parsing fails on one file (old-style scanlines), the flat-scanline branch handles it; if dimensions look transposed, the file is `+Y`-first — stop and inspect the header.

- [ ] **Step 3: Commit.**
```bash
git add scripts/tools/env_peakiness.py results/campaign/env_peakiness.csv
git commit -m "Add env peakiness metrics script + CSV (fig:ris-ksweep x-axis provenance)"
```

---

### Task 4: Calibrated builds for all four assets (the TUNED stash)

**Files:** none committed (binaries); run-log notes in the task's record

- [ ] **Step 1: Build + stash each asset's calibrated pair.** `calibrate_caps.sh` measures+writes+rebuilds+verifies in one go (defaults: 16 spp, seeds 42 43, verify seed 7):
```bash
mkdir -p ~/winbins
for a in cloud tornado explosion bunny; do
  scripts/tools/calibrate_caps.sh $a 2>&1 | tail -3        # expect: verify OK — no overflow
  grep -E "MAX_ACTIVE_PRIMS = |HIT_BUFFER_CAPACITY = " device/core/constants.cuh
  cp build/bin/Release/test_runner ~/winbins/exe_$a
  cp build/device_program.optixir  ~/winbins/ir_$a
done
git checkout device/core/constants.cuh && cmake --build build --target test_runner -j >/dev/null 2>&1
cp build/bin/Release/test_runner ~/winbins/exe_stock; cp build/device_program.optixir ~/winbins/ir_stock
```
Expected constants per asset: cloud 64/96, tornado 112/384, explosion 32/160, bunny 80/528 (if calibration suggests different numbers than `cap_calibration.md`, STOP — investigate before timing anything).

- [ ] **Step 2: Furnace-gate each stashed pair** (cap-independent correctness):
```bash
for a in cloud tornado explosion bunny; do
  cp ~/winbins/exe_$a build/bin/Release/test_runner; cp ~/winbins/ir_$a build/device_program.optixir
  SG_ALBEDO=1.0 build/bin/Release/test_runner --scene single_gaussian_validation --spp 1024 >/dev/null 2>&1
  echo -n "$a furnace: "; tools/refs/.venv/bin/python tools/refs/furnace_check.py \
    test_results/single_gaussian_validation/0000.exr | grep -oE "=>.*"
done
cp ~/winbins/exe_stock build/bin/Release/test_runner; cp ~/winbins/ir_stock build/device_program.optixir
```
Expected: 4× `=> PASS`.

Use a pair via: `cp ~/winbins/exe_<a> build/bin/Release/test_runner; cp ~/winbins/ir_<a> build/device_program.optixir`. ALWAYS restore the stock pair when a task ends.

---

### Task 5: G3 flat rung (RIS on constant env — the scene-dependence anchor)

**Files:**
- Create: `results/campaign/ris_seeds_flat/` (gitignored) + extend `results/campaign/ris_ksweep.md`

- [ ] **Step 1: Gitignore + run the sweep** (calibrated cloud pair; protocol R7; no `SG_ENV` = white_constant):
```bash
grep -qxF "results/campaign/ris_seeds_flat/" .gitignore || echo "results/campaign/ris_seeds_flat/" >> .gitignore
mkdir -p results/campaign/ris_seeds_flat
cp ~/winbins/exe_cloud build/bin/Release/test_runner; cp ~/winbins/ir_cloud build/device_program.optixir
OUT=results/campaign/ris_seeds_flat
echo "arm,seed,time_s" > $OUT/times.csv
for s in $(seq 1 16); do
  for arm in mis 1 2 4 6 8 12; do
    [ "$arm" = mis ] && FLAGS="" || FLAGS="--ris --ris-candidates $arm"
    t=$(SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering --spp 64 --seed $s $FLAGS 2>&1 \
        | grep -oE "Total time: [0-9.]+s" | grep -oE "[0-9.]+")
    cp test_results/cloud_asset_scattering/0000.exr $OUT/${arm}_s${s}.exr
    echo "$arm,$s,$t" >> $OUT/times.csv
  done; echo "seed $s done"
done
```
(~15–20 min; run the Task 0 clock sentinel alongside.)

- [ ] **Step 2: Extract.**
```bash
tools/refs/.venv/bin/python scripts/tools/extract_k.py --dir results/campaign/ris_seeds_flat \
  --arms mis 1 2 4 6 8 12 --seeds 1 16 --spp 64 --times results/campaign/ris_seeds_flat/times.csv --base mis
```
Expected shape: **RIS loses on flat** — speedups < 1 for all K (dev-era estimate ~0.4×, i.e. ~2.5× worse). If RIS *wins* on flat, something is wrong (probably SG_ENV leaked) — check the render log's env line.

- [ ] **Step 3: Record + commit.** Append a "Flat rung" section to `results/campaign/ris_ksweep.md` (same table format as the meadow section, plus the clock-sentinel min/p50/max), then:
```bash
git add results/campaign/ris_ksweep.md .gitignore
git commit -m "G3 flat rung: RIS loses on constant env (measured), scene-dependence anchored"
```

---

### Task 6: G3 studio rung

Same as Task 5 with `SG_ENV=studio`, output dir `results/campaign/ris_seeds_studio/`, and expectation **between** flat and meadow (dev never measured this point — let the data speak; plausible range 1.0–1.4×). Record section "Studio rung"; commit message `"G3 studio rung: mid-peakiness RIS point measured"`.

---

### Task 7: Assemble fig:ris-ksweep (3 envs) + meadow timing re-anchor

**Files:**
- Modify: `results/campaign/ris_ksweep.csv` (currently header-only), `results/campaign/ris_ksweep.md`
- Regenerate: `thesis/latex/figures/ris_ksweep.pdf`

- [ ] **Step 1: Meadow timing-only re-anchor at calibrated caps.** Banked meadow k stays (rule R8); re-measure times only, 5 interleaved rounds:
```bash
cp ~/winbins/exe_cloud build/bin/Release/test_runner; cp ~/winbins/ir_cloud build/device_program.optixir
for r in 1 2 3 4 5; do for arm in mis 1 2 4 6 8 12; do
  [ "$arm" = mis ] && FLAGS="" || FLAGS="--ris --ris-candidates $arm"
  t=$(SG_ENV=meadow SG_CAM=0 build/bin/Release/test_runner --scene cloud_asset_scattering --spp 64 --seed 1 $FLAGS 2>&1 \
      | grep -oE "Total time: [0-9.]+s" | grep -oE "[0-9.]+")
  echo "round=$r arm=$arm t=$t"
done; done
```
Compute median per arm; speedup_meadow = (k_mis·t_mis)/(k_K·t_K) with banked k from `ris_ksweep_meadow.csv`. Sanity: should land within ~0.05 of the banked speedups (1.48 @ K=4 etc.); if it shifts more, the calibrated build moved timing — note it, the new numbers win.

- [ ] **Step 2: Fill the figure CSV** (`K,speedup_flat,speedup_studio,speedup_meadow`, K rows 1,2,4,6,8,12) from Tasks 5/6/this. Then:
```bash
bash scripts/plots/build_figures.sh 2>&1 | grep ris_ksweep   # expect: "wrote ... (6 points, y=[...3 cols...])"
```

- [ ] **Step 3: Thesis check + commit.** `sec:ris` already cites 1.48×/plateau; verify `fig:ris-ksweep`'s caption claims match the measured flat/studio numbers (edit `thesis/latex/chapters/06-optimization.tex` caption if needed); `latexmk` clean; commit CSV+md+figures+any tex: `"Complete fig:ris-ksweep: 3-env RIS curve measured (flat loses, studio X, meadow 1.48x)"`.

---

### Task 8: RR timing-only re-run (clears the "provisional" flag)

**Files:**
- Modify: `results/campaign/rr_depth.csv` (t_abs/eff columns), `rr_depth.md`, spec §G2 note
- Regenerate: `thesis/latex/figures/rr_depth.pdf`

- [ ] **Step 1: Time-only sweep** (banked k reused, R8), calibrated cloud pair, 5 interleaved rounds × depths {5,6,8,10,12,16}, meadow scattering, seed 1, same command pattern as Task 7 Step 1 with `--rr-depth $d` instead of RIS flags.
- [ ] **Step 2: Update** `rr_depth.csv`'s `t_abs_clean_s` and `eff`. Medians from the captured `round=... depth=... t=...` lines:
```bash
# per depth: median of the 5 rounds
for d in 5 6 8 10 12 16; do
  grep "depth=$d " /path/to/rr_rerun.log | grep -oE "t=[0-9.]+" | cut -d= -f2 | sort -n | sed -n 3p \
    | xargs -I{} echo "depth $d median {}s"
done
```
Recompute `t_rel` (depth median ÷ mean of all depth medians) and `eff = k × t_rel` with the banked k column. Confirm the basin shape is unchanged (min still in 8–12; if the nominal min moves within the basin, that's fine — it's a plateau).
- [ ] **Step 3: Regenerate the figure; soften/remove the "provisional absolutes" caveats** in `rr_depth.md` + spec §G2; `latexmk` clean if the Ch 6 numbers change (they shouldn't — prose cites +3.4 % and the basin, which are k-dominated); commit: `"RR-depth absolutes re-anchored at calibrated caps on a quiet GPU (provisional flag cleared)"`.

---

### Task 9: G2 merge-commit ladder (the four historical wins) + fast-erf + denoiser

**Files:**
- Create: `results/campaign/wins.csv` (`optimization,semantics,frame_s_before,frame_s_after,speedup,k_note`), `results/campaign/wins_ladder.md`

**Ladder pairs (BEFORE vs AFTER, both built fresh):**

| win | BEFORE | AFTER |
|---|---|---|
| shadow-transmittance | `git merge-base main deprecated-shadow-transmittance-opt` | `deprecated-shadow-transmittance-opt` (tip `8c12af5`) |
| skip per-bounce scan | `174777d~1` | `174777d` |
| dedup bounce-0 | `f54deaa~1` | `f54deaa` |
| any-hit fusion | `71ced87~1` | `71ced87` |

- [ ] **Step 1: For each pair, build both sides into stashed pairs.** Use a scratch worktree so `build/` stays on the campaign binary:
```bash
git worktree add /home/kacper/ladder <SHA>     # one side at a time
cd /home/kacper/ladder && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null && \
  cmake --build build --target test_runner -j >/dev/null 2>&1
cp build/bin/Release/test_runner ~/winbins/ladder_<win>_<side>; cp build/device_program.optixir ~/winbins/ladder_ir_<win>_<side>
cd /home/kacper/thesis && git worktree remove --force /home/kacper/ladder
```
CAVEAT: historical CLIs differ. Default invocation: `SG_ENV=meadow SG_CAM=0 ./build/bin/Release/test_runner --scene cloud_asset_scattering --spp 64 --seed 1`. After each build run `--help`; if a flag/scene is absent on either side of a pair, drop to the heaviest scene+flags that exist on BOTH sides (the A/B only needs internal consistency — both sides identical). Document the chosen invocation per pair in `wins_ladder.md`. NOTE: each ladder exe bakes ITS OWN worktree optixir path which no longer exists after worktree removal — so stash the ir and **copy it to the baked path**? No: simpler — keep the worktree alive until that pair's A/B is done, run the exe from inside the worktree, THEN remove it.
- [ ] **Step 2: Interleaved A/B per pair** (≥3 alternating rounds, identical invocation, log Total time; clock sentinel running). Record medians + the historical FINDINGS number next to it.
- [ ] **Step 3: fast-erf pair at final:** `cmake -S . -B build-ferf -DCMAKE_BUILD_TYPE=Release -DTHESIS_ENABLE_FAST_ERF=ON`, build, interleave vs the stock pair on cloud-meadow @64 spp ×5 rounds; also a converged-mean bias check (1024 spp, `exr_diff.py` vs stock render — expect mean-equal at ≤1e-5).
- [ ] **Step 4: Denoiser = effective, not k:** render GT `--spp 2048` (meadow cloud, seed 99), then `--spp 64` raw and `--spp 64 --denoise`; `tools/refs/exr_rmse.py <render> <GT>` for both → `results/campaign/effective.csv` rows (`feature,scene,rmse_vs_gt,time_s`).
- [ ] **Step 5: Bank `wins.csv` + `wins_ladder.md`** (semantics column: `sequential-historical` for the 4 ladder rows, `marginal-at-final` for fast-erf/denoiser); update `tab:wins` in `thesis/latex/chapters/06-optimization.tex` if any measured number deviates >2× from the cited dev-era effect (precedent: RR's 11 %→3.4 % was reported honestly — do the same); `latexmk`; commit per result or one ladder commit.

---

### Task 10: Mitsuba-parity gates for tornado/explosion (+ bunny camera check)

**Files:**
- Create: `results/campaign/asset_parity.md`

- [ ] **Step 1: Read the template** — §8.25 energy-ratio method: `grep -n "8.25" thesis/FINDINGS.md | head` and read that section; harness = `tools/refs/render_asset_via_prb.py` (env: `SG_PLY`, `SG_VIEW`, constant env first; runs via `tools/refs/with_jorge_mitsuba.sh`).
- [ ] **Step 2: Per asset (tornado, explosion):** render ours (calibrated pair, `asset_validation`, `SG_ALBEDO=0` absorption first, 256 spp) and Mitsuba's (same view/albedo); compare with `tools/refs/exr_diff.py` — gate: converged-mean ratio within ~1e-3. KNOWN ISSUE: the asset camera was found vertically flipped vs Mitsuba — the *energy ratio* (means) is flip-invariant, so gate on means first; for the per-pixel diff, flip ours with:
```bash
tools/refs/.venv/bin/python - results/campaign/ours.exr results/campaign/ours_flipped.exr <<'PY'
import sys, OpenEXR, Imath, numpy as np
src, dst = sys.argv[1], sys.argv[2]
f = OpenEXR.InputFile(src); h = f.header(); dw = h['dataWindow']
W, H = dw.max.x - dw.min.x + 1, dw.max.y - dw.min.y + 1
pt = Imath.PixelType(Imath.PixelType.FLOAT)
ch = {c: np.frombuffer(f.channel(c, pt), np.float32).reshape(H, W)[::-1].copy() for c in ('R', 'G', 'B')}
out = OpenEXR.OutputFile(dst, OpenEXR.Header(W, H))
out.writePixels({c: ch[c].tobytes() for c in ('R', 'G', 'B')}); out.close()
print("wrote", dst)
PY
```
Optional add-on while in the Mitsuba harness: the `SG_SHAPE=ellipsoids` reference-side re-gate from `icosphere_port.md` (one cloud render with the analytic reference shell; does the energy agreement tighten vs the default 72△ uv_sphere shell?).
- [ ] **Step 3:** PASS → bunny/tornado/explosion are eligible for cross-renderer claims (G1 bunny rung); FAIL → scope those assets ours-internal and say so in the record. Commit `asset_parity.md`.

---

### Task 11: G1 headline (the long pole — needs the cleanest GPU; do last among timed tasks)

**Files:**
- Create: `results/campaign/headline.csv` (`renderer,asset,env,config,spp,t_med_s,k,k_clip,p99_9,p99_99,maxpix`), `results/campaign/headline.md`, money-shot EXRs/PNGs under `results/campaign/showcase/`

- [ ] **Step 1: Locate + gate the BARE baseline.**
```bash
BARE=$(git merge-base main deprecated-shadow-transmittance-opt); echo $BARE
git show $BARE:device/core/constants.cuh | grep -E "RR_DEPTH"      # expect: = 5 (historical default)
```
Build BARE in a worktree (Task 9 pattern, keep worktree alive); furnace-gate it (whatever furnace path exists at that commit — check `test/` there).
- [ ] **Step 2: Flat rung, three arms** (white_constant, cloud): (a) BARE, (b) final-validation (calibrated cloud pair), (c) Mitsuba-analog. For (a)/(b): 16 seeds × 64 spp, k via `extract_k.py`, times from logs. For (c): `tools/refs/cloud_meadow_seeds.sh` is the harness pattern (CUDA+Mitsuba halves, resumable; analog = NEE off is the default per its comments). First `head -60` it and its Mitsuba callee to see the env handling; if it is meadow-hardwired, the fallback is `tools/refs/render_via_volprim.py` (it builds the scene dict programmatically — swap the emitter for a constant; verify with `head -60`) driven in a 16-seed loop. Mitsuba seeds: 16; pin and record the Mitsuba spp; per-spp steady-state time excludes JIT on both sides (G7 statement, `jit_overhead.md`).
  Deliverable: the **deficit-closure number** — (k·t) ratios bare→final and final-vs-Mitsuba-analog on flat. Dev-era anchor: ~5.5× closed (1.93× per-spp × 2.85× noise); measure, don't assume.
- [ ] **Step 3: Showcase rung (meadow):** final-validation vs Mitsuba-analog equal-quality (16 seeds each; clipped-k at 99.9 % + p99.9/p99.99/max per R7 + spec §4 tails note); Mitsuba-MIS arm for per-spp time + firefly stats + bias ONLY (never equal-quality — it converges to a biased image, +155 % on showcase).
- [ ] **Step 4: Bunny rung** only if Task 10 passed for cross-renderer; else ours-internal scaling (bunny calibrated pair, 16 seeds, k+t — no Mitsuba claim).
- [ ] **Step 5: Money shots** (final-showcase config = `--denoise`, beauty settings): ours vs Mitsuba-analog at equal quality + a firefly crop vs Mitsuba-MIS. Save under `results/campaign/showcase/`.
- [ ] **Step 6: Bank** `headline.csv` + `headline.md` (with clock-sentinel stats + GPU state log); commit. Thesis wiring: Ch 6 intro `06:57-59` ("the equal-quality comparison inverts") now cites this run; Ch 7 headline numbers — update `thesis/latex/chapters/06-optimization.tex` + `07-results.tex`, `latexmk` clean, commit separately.

---

### Task 12: G4 bunny ncu profile (clock-independent — runnable even at 150 W)

**Files:**
- Modify: `results/campaign/ncu_summary.md` (add bunny section), `results/campaign/roofline.csv` (bunny row)

- [ ] **Step 1:** Follow the exact recipe pinned in `results/campaign/ncu_summary.md` (sections Occupancy/SchedulerStats/WarpStateStats/SpeedOfLight; FLOPs via pipe counters — SASS counters return 0 on OptiX; ncu base-clocks itself). Bunny calibrated pair, `asset_validation`, `SG_RES=256`, `SG_ALBEDO=0.9`, meadow, `--spp 4`, `--launch-count 1`.
- [ ] **Step 2:** Append the bunny row/section; note deltas vs cloud (registers, occupancy, stall mix — bunny's 528-deep hit buffer is the interesting contrast). Regenerate roofline if the bunny point is added to the figure. Commit.

---

### Task 13: G6 confirms (wavefront point + adaptive effective)

**Files:**
- Create: `results/campaign/g6_confirms.md`; extend `effective.csv`

- [ ] **Step 1: Wavefront one-point confirm:** worktree at `deprecated-wavefront-phase1` tip; the toggle is in its `cmake/Device.cmake`/`OptiX-IR.cmake` (`THESIS_WAVEFRONT`). Build ON and OFF **at that same commit**, interleave 3 rounds on the heaviest scene that branch supports. Expect OFF ≫ ON (dev range 100–1400×; one confirming point suffices — even a 64² render).
- [ ] **Step 2: Adaptive effective:** `sed -i 's/ENABLE_ADAPTIVE_SAMPLING = false/ENABLE_ADAPTIVE_SAMPLING = true/' device/core/constants.cuh` (+ set `ADAPTIVE_THRESHOLD = 0.01f`), rebuild, render meadow cloud at matched wall-time vs uniform, RMSE both vs the Task 9 GT (`exr_rmse.py`) → `effective.csv`. Expect adaptive worse-or-equal at equal time (dev: ~2× slower at equal quality). RESTORE constants (R4).
- [ ] **Step 3:** Bank + commit.

---

### Task 14: Close-out

- [ ] **Step 1:** `bash scripts/plots/build_figures.sh` — every placeholder that has data becomes real; `latexmk` clean; check no `PROVISIONAL` watermark remains in figures the text cites as measured.
- [ ] **Step 2:** Update `docs/superpowers/HANDOFF.md` banked table + spec status blocks (G1–G4, G6 → DONE with one-line findings + record paths).
- [ ] **Step 3:** Final commit; remind Kacper: merge of `feature/icosphere-gas` → `main` is his call, and `deprecated-*`/worktree cleanup is his.
- [ ] **Step 4: Out-of-scope leftovers — surface, don't do:** Ch 5 validation montages (`fig:absorption-ladder` / `fig:scattering-ladder` / `fig:showcase` — assembled from Task 10/11 renders + Ch 5 dev artifacts; a figure-assembly job, not an experiment); spec §8 open decisions for Kacper (R3 `stress_N` sweep, Mitsuba-side peak VRAM, explosion no-emission look with Jorge); G7 #96 prose folding into Ch 3/Ch 7. List them in the handoff with pointers.

## Effort/GPU-time budget (locked clocks, uncontended)

| Task | wall time |
|---|---|
| 0–4 (prep+builds+gates) | ~1.5 h (mostly builds) |
| 5+6 (flat+studio rungs) | ~40 min |
| 7+8 (re-anchors+figure) | ~30 min |
| 9 (ladder ×4 + fast-erf + denoiser GT) | ~2–3 h (builds dominate) |
| 10 (parity gates) | ~45 min (Mitsuba renders) |
| 11 (headline, both rungs + Mitsuba seeds) | ~3–5 h (Mitsuba-analog meadow is the pole) |
| 12 (ncu bunny) | ~30 min |
| 13 (G6) | ~1 h |
| **Total** | **~10–12 h** → plan an overnight window; Tasks 5–9 and 11 need it, 10/12 don't |

---

## Deprecated-branch conclusions — cap-staleness audit (added 2026-06-13)

The ~18 `deprecated-*` branches carry documented conclusions measured at OLD caps (stock 128/128 or the older estimator), not the calibrated per-asset caps. Per rule **R8**, a number is **cap-sensitive only if it is an absolute timing**; equal-quality *ratios* (both arms share the build), *counts*, *accuracy/RMSE*, and *huge-margin negatives* are cap-robust (image class is bit-identical at 0 overflows; ratios shift together). Audit (numbers from `thesis/FINDINGS.md` §8.x):

| Branch | Feature | Verdict | Documented № | Type | On | Src | Cap-sensitive? |
|---|---|---|---|---|---|---|---|
| shadow-transmittance-opt | per-prim shadow τ (no sort) | win | ~12–15× shadow kernel; ~7× per-spp | TIMING | cloud const | §8.16-17 | **yes → G2 ladder** |
| incremental-active-prims (skip-scan) | reuse bounce-0 active set | win | **~16%** | TIMING | cloud | §8.23 | **yes (most) → G2 ladder** |
| dedup-bounce0-scan | dedup origin-inside set | win | ~8% | TIMING | cloud | §8.19 | **yes → G2 ladder** |
| anyhit-transmittance-fusion | shadow τ in anyhit | win | ~3% | TIMING | cloud 128spp | §8.18 | **yes → G2 ladder** |
| fast-erf | approx erf hot path | win (opt-in) | ~1.5% | TIMING | cloud meadow | §8.21 | **yes → G2b** |
| volumetric-ris | product-RIS direct light | win (gated) | ~1.4× env; ~2.5× worse flat | RATIO | cloud meadow/flat | §8.37 | ratio-robust → **G3 re-measures** |
| wavefront / -phase1 | host-driven per-bounce | rejected | 100–1400× slower | TIMING (huge) | single-G, asset | §8.34 | no (margin) → G6 confirm |
| adaptive-sampling | per-pixel stop | rejected | ~2× slower; −5e-4 bias | RATIO | cloud σ7.5 | §8.30 | no (margin) → G6 confirm |
| a1-per-step-rb | per-step Rao-Black. | rejected | ~3× var flat-only, 0× showcase | RATIO | single-G, cloud | §8.27,8.5 | no (ratio, dead-end) |
| sobol-sampling | Owen-scrambled Sobol' | rejected | −0.1%..+0.8% RMSE (no win) | RMSE | cloud meadow | §8.20 | no (ratio) |
| env-is-alias-table | Walker/Vose alias | deferred | <1% (−0.8..+0.4%) | TIMING | cloud meadow | §8.36 | no (sub-jitter, dropped) |
| denoiser | OptiX HDR denoiser | win | ~30× effective | RMSE ratio | cloud 16 vs 512 | §8.22 | no (ratio) → G2b re-touches |
| asset-nan | negative-t guard | bugfix | NaN 8→0 | COUNT | embergen 24k | §8.26 | no (count) |
| robustness-fixes | uint32 spp + overflow detect | bugfix | wrap fixed | COUNT | cloud | §9 | no (count) |
| showcase-quality | firefly clamp + recon filter | win (beauty) | firefly max↓1.36 | COUNT/qual | cloud meadow | §8.24 | no (count/image-only) |
| path-guiding | learned 32³ guide | deferred | no payoff | diagnostic | scaffold | §8.38 | no |
| analog-indirect-diagnostic | variance-gap diagnostic | diagnostic | estimator id'd | RATIO | single-G | §8.27 | no |

**Verdict — the cap-sensitive conclusions are already in the lineup.** Every deprecated number that is a genuine cloud absolute-timing speedup is re-measured at calibrated caps by a planned experiment: shadow-transmittance, skip-scan, dedup-bounce0, anyhit-fusion → **G2 merge-ladder (Task 9)**; fast-erf → **G2b (Task 9 Step 3)**; volumetric-RIS → **G3 (Tasks 5–7)**; wavefront, adaptive → **G6 (Task 13)**. **No extra reruns are needed for the rest** — they are cap-robust (counts, accuracy/RMSE ratios, equal-quality ratios, huge-margin negatives, or the sub-jitter/dropped alias table).

**One to watch in G2 (Task 9):** `incremental-active-prims` (~16%, §8.23) is the *most* cap-coupled conclusion — it removes a per-bounce scan over the active-prim set, whose cost scales with `MAX_ACTIVE_PRIMS`. Cloud's active cap dropped **128→64**, so the scan it skips is now half as large and the ~16% may shrink. Its ladder pair (`174777d~1 ↔ 174777d`) measures the calibrated-cap value directly — report honestly if it deviates >2× (precedent: RR's 11%→3.4%).
