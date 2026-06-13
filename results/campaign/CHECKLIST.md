# Section-6 Experiment Campaign — Running Checklist

**Updated:** 2026-06-13 11:40 CEST · **Legend:** `[x]` done · `[ ]` pending (tag shows *when*: ← running / ← this window / ← post-window 150W / ← next window)

Source of truth for progress. Full procedure: `docs/superpowers/plans/2026-06-12-section6-window-campaign.md`. Records: `results/campaign/*.md`.

---

## A. Setup & prep
- [x] `extract_k.py` (regression-validated: reproduces banked d12 k=1.98417)
- [x] `env_peakiness.py` + `env_peakiness.csv` (white 1×/0.6% · studio 538.8×/39.7% · meadow 1.53e5×/78.1%)
- [x] studio env wiring (`SG_ENV=studio` → ferndale_studio_01_4k)
- [x] calibrated builds stashed in `~/winbins/`, all furnace-PASS: **cloud 64/96 · tornado 112/384 · explosion 32/160 · bunny 80/528**
- [x] fast-erf build (`build-ferf/`, caps 64/96, `THESIS_ENABLE_FAST_ERF=ON`)
- [x] `main` fast-forwarded to campaign HEAD; plan updated (exec split + deprecated audit)

## B. GPU / bench settings
- [x] 350 W · SM 1800 · mem 9751 · persistence (verified live)
- [ ] CPU governor=performance  ← before G1 (Mitsuba/absolute numbers)

## C. Experiments

### G1 — headline equal-quality vs Mitsuba  ← next window (3–5 h, cleanest GPU)
- [ ] Mitsuba pre-flight (verify `cloud_meadow_seeds.sh`; match depth/res/cam/env; `SG_SHAPE=ellipsoids` re-gate)  ← post-window
- [ ] flat rung (bare → final-validation → Mitsuba-analog)
- [ ] meadow showcase rung (+ Mitsuba-MIS for time/firefly/bias only)
- [ ] bunny rung (gated on G10 parity)
- [ ] money shots (final-showcase)

### G2 — optimization ablations → `tab:wins`, `fig:rr-depth`
- [ ] RR-depth timing re-anchor (k banked R8)  ← **running now**
- [ ] merge-ladder: shadow-transmittance · skip-scan · dedup-bounce0 · any-hit fusion  ← needs worktree builds (post-window) then next window
- [ ] fast-erf A/B + bias check  ← **this window** (build-ferf ready)
- [ ] denoiser effective-RMSE vs 2048-spp GT

### G3 — RIS scene-dependence → `fig:ris-ksweep`
- [x] meadow rung (2026-06-12; 1.48× at K=4–6, CI'd; k banked)
- [ ] meadow timing re-anchor  ← **running now**
- [ ] flat rung (expect RIS loses)  ← **running now** (seed 9/16)
- [ ] studio rung (expect mid)  ← this window (queued in batch)
- [ ] assemble `fig:ris-ksweep` (3 envs)  ← post-batch analysis

### G4 — bottleneck profile (ncu)
- [x] cloud profile (`ncu_summary.md`, `roofline.csv`)
- [ ] bunny profile  ← post-window 150W (ncu self-locks clocks)

### G5 — GAS / VRAM memory
- [x] done (`gas_memory.csv`; cap-independent)

### G6 — negative-result confirms
- [ ] wavefront ON/OFF (one point)  ← post-window 150W
- [ ] adaptive effective  ← post-window 150W

### G7 — Mitsuba JIT/startup (#96)
- [x] measured (`jit_overhead.md`: ours 0.39 s vs Mitsuba 0.78 s warm / 2.20 s cold)
- [ ] wire numbers into Ch 3 + results/limitations (startup/iteration-latency advantage)  ← post-batch thesis edits

### G8 — analytic vs icosphere
- [x] accuracy (3 assets, RMSE)
- [x] perf (cloud, 128/128 — ratio cap-robust)
- [ ] perf re-anchor at calibrated caps  ← optional/cosmetic

### G10 — Mitsuba parity gates (cross-renderer eligibility)
- [ ] tornado parity (energy-ratio, absorption)  ← post-window 150W (power-immune)
- [ ] explosion parity  ← post-window 150W

## D. Thesis wiring (figures / tables / prose)
- [ ] `fig:ris-ksweep` (after G3 flat+studio)
- [ ] `rr_depth.csv` absolutes + `fig:rr-depth` (after RR re-anchor)
- [ ] `tab:wins` rows: RR · RIS · fast-erf · ladder
- [ ] Ch 3 JIT sentence → measured magnitude; G7 line in results/limitations
- [ ] `sec:bottleneck` bunny + `roofline.pdf` (after G4b)

## E. Deprecated-branch reruns
- [x] cap-staleness audit done — **no extra reruns**: cap-sensitive numbers are already covered by G2/G3/G6; the rest are cap-robust (ratios/counts/RMSE/huge-margin). **Watch:** `incremental-active-prims` (~16%, §8.23) in the G2 ladder may shrink at the smaller active cap.

## Standing protocol
- [x] every render checked for `Cap overflow` (driver-enforced); act on any hit (Kacper's directive)
- next action: monitor pings ~11:53 → analyze batch, build `fig:ris-ksweep`, do Ch3/G7 thesis edits, then fast-erf A/B in-window.
